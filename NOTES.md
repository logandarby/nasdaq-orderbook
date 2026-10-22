# Lab log

Log of all stages of implementation, as well as performance experiments attempted.

This document is not AI-generated. 

Date format is YYYY-MM-DD.

## 2026-10-22 Stage 1: ITCH Parser

The goal of this stage is to get an ITCH parser working, and make it correct.

Goal: parse a full trading day from file, with add, execute, cancel, delete, replace and report messages per second.

We will be using data from 30 Dec 2019 (12302019). This will be our tuning day, we will use a separate day for performance testing once we get there.

Done When:
  - full day parses, message types handled
  - fuzzer runs clean under ABsan, UBsan
  - counts are reported

<!-- One entry per experiment. Write the hypothesis and the prediction before running it.
Wrong guesses stay in.

## Template

### YYYY-MM-DD: <experiment>

- **Hypothesis:**
- **Prediction (before running):**
- **Setup:** commit, preset, results directory
- **Measured:**
- **Conclusion:** -->
