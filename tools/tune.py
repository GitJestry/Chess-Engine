#!/usr/bin/env python3
"""End-to-end evaluation parameter tuner.

This script generates training data via Stockfish self play, labels positions
using Stockfish evaluations converted to win/draw/loss probabilities and tunes
engine evaluation parameters using Texel loss with a simple SPSA optimiser.

The tunable parameters live in ``include/lilia/engine/eval_tune_shared.hpp``.
After optimisation the resulting parameters can be written back as ``constexpr``
values to be used for normal engine builds.

Note: The default configuration is intentionally conservative so that the script
can be executed in a restricted environment.  For large scale tuning (1M+
positions) increase the ``--samples`` argument accordingly.
"""

from __future__ import annotations

import argparse
import math
import os
import random
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

import numpy as np
import chess
import chess.engine

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

HEADER_PATH = Path(__file__).resolve().parent.parent / "include" / "lilia" / "engine" / "eval_tune_shared.hpp"


@dataclass
class Parameters:
    values: Dict[str, float]

    @classmethod
    def load(cls) -> "Parameters":
        text = HEADER_PATH.read_text(encoding="utf-8")
        pattern = re.compile(r"inline\s+int\s+(\w+)\s*=\s*([-0-9]+)")
        values = {m.group(1): float(m.group(2)) for m in pattern.finditer(text)}
        return cls(values)

    def save_as_constexpr(self, out_path: Path) -> None:
        content_lines = [f"constexpr int {k} = {int(round(v))};" for k, v in self.values.items()]
        out_path.write_text("\n".join(content_lines) + "\n", encoding="utf-8")


# Texel formula converting centipawns to win probability.
def cp_to_wdl(cp: float) -> float:
    return 1.0 / (1.0 + math.exp(-cp / 400.0))


# Simple material based evaluation using a subset of parameters.
def evaluate_board(board: chess.Board, params: Parameters) -> float:
    p = params.values
    piece_values = {
        chess.PAWN: p.get("PAWN_VALUE", 100),
        chess.KNIGHT: p.get("KNIGHT_VALUE", 320),
        chess.BISHOP: p.get("BISHOP_VALUE", 330),
        chess.ROOK: p.get("ROOK_VALUE", 500),
        chess.QUEEN: p.get("QUEEN_VALUE", 900),
        chess.KING: 0,
    }
    score = 0.0
    for piece_type in piece_values:
        score += piece_values[piece_type] * (
            len(board.pieces(piece_type, chess.WHITE))
            - len(board.pieces(piece_type, chess.BLACK))
        )
    # Tempo bonus
    score += p.get("TEMPO_MG", 0) if board.turn == chess.WHITE else -p.get("TEMPO_MG", 0)
    # Bishop pair
    if len(board.pieces(chess.BISHOP, chess.WHITE)) >= 2:
        score += p.get("BISHOP_PAIR", 0)
    if len(board.pieces(chess.BISHOP, chess.BLACK)) >= 2:
        score -= p.get("BISHOP_PAIR", 0)
    return score


# Generate FEN positions via Stockfish self play.
def generate_fens(engine_path: str, samples: int) -> List[str]:
    engine = chess.engine.SimpleEngine.popen_uci(engine_path)
    fens = []
    board = chess.Board()
    while len(fens) < samples:
        result = engine.play(board, chess.engine.Limit(depth=1))
        board.push(result.move)
        fens.append(board.fen())
        if board.is_game_over():
            board.reset()
    engine.quit()
    return fens


# Label positions with Stockfish evaluation converted to WDL probability.
def label_fens(engine_path: str, fens: Iterable[str]) -> List[float]:
    engine = chess.engine.SimpleEngine.popen_uci(engine_path)
    labels = []
    for fen in fens:
        board = chess.Board(fen)
        info = engine.analyse(board, chess.engine.Limit(depth=10))
        cp = info["score"].white().score(mate_score=10000)
        labels.append(cp_to_wdl(float(cp)))
    engine.quit()
    return labels


# Texel loss (logistic cross entropy).
def texel_loss(pred: np.ndarray, target: np.ndarray) -> float:
    eps = 1e-12
    return float(-np.mean(target * np.log(pred + eps) + (1 - target) * np.log(1 - pred + eps)))


# SPSA optimiser.
def spsa_optimize(params: Parameters, fens: List[str], labels: List[float],
                  iterations: int = 100, a: float = 0.1, c: float = 0.1) -> Parameters:
    keys = list(params.values.keys())
    theta = np.array([params.values[k] for k in keys], dtype=float)
    labels_np = np.array(labels, dtype=float)

    def evaluate(theta_vec: np.ndarray) -> float:
        p_dict = {k: v for k, v in zip(keys, theta_vec)}
        local_params = Parameters(p_dict)
        preds = [cp_to_wdl(evaluate_board(chess.Board(f), local_params)) for f in fens]
        return texel_loss(np.array(preds), labels_np)

    for k in range(1, iterations + 1):
        alpha = a / (k ** 0.602)
        gamma = c / (k ** 0.101)
        delta = np.array([random.choice([-1, 1]) for _ in theta])
        loss_plus = evaluate(theta + gamma * delta)
        loss_minus = evaluate(theta - gamma * delta)
        grad = (loss_plus - loss_minus) / (2.0 * gamma * delta)
        theta -= alpha * grad
    tuned = Parameters({k: v for k, v in zip(keys, theta)})
    return tuned


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description="End-to-end SPSA tuner")
    parser.add_argument("--stockfish", default="stockfish", help="Path to Stockfish binary")
    parser.add_argument("--samples", type=int, default=100, help="Number of FEN samples to generate")
    parser.add_argument("--iterations", type=int, default=50, help="Number of SPSA iterations")
    parser.add_argument("--export", type=Path, help="Export tuned parameters to header")
    args = parser.parse_args()

    params = Parameters.load()
    fens = generate_fens(args.stockfish, args.samples)
    labels = label_fens(args.stockfish, fens)
    tuned = spsa_optimize(params, fens, labels, iterations=args.iterations)

    if args.export:
        tuned.save_as_constexpr(args.export)
        print(f"Exported tuned parameters to {args.export}")
    else:
        for k, v in tuned.values.items():
            print(f"{k} = {v:.2f}")


if __name__ == "__main__":
    main()
