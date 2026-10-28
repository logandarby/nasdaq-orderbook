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
  - unit tests pass
  - fuzzer runs clean under ASan, UBsan
  - counts are reported

### 2026-10-27 Parser Output

3 Options for output:
1. Store only a byte array, and offer convenience functions for decoding (Not type safe)
2. Store a byte array in a named struct, with appropriate methods to decode fields. Use in a variant for compiler type-safety
3. Store output in a decoded struct-- no raw bytes. Use in a variant

- Decision: Number 2
- Reason: Offers nice balance of lazy views, storage, and type safety. The variant makes it type safe, but storing only the byte array ensures that each variant is roughly the same size-- fully packed, to perhaps take better advantage of cache. Also, we can choose which fields we want to decode instead of decoding them all simultaneously. 
- Tradeoffs: 
  - Option 3 is typesafe (like 2, but unlike 1), and it is arguably the most readable, but it costs more space (largest struct in variant) and risks extra work decoding fields we don't need. Its pro is that we don't need lazy decoding, or duplicate decoding.
  - Option 1 isn't type safe, and could be surface area for nasty bugs, but it might be more performant
  - The con of option 2 is that a field that gets read twice gets decoded twice, and there's a copy per message, but I think this is fine.

I hypothesize option 2 will be faster than option 3 by some reasonable metric. The exact design is not a concern now, and can be measured later if it's identified as a bottleneck.

Notes:
  - The file format uses the MoldUDP64, which appends 2-byte lengths to every sentence. We will be ignoring this in the parser

### 2026-10-28 Parser Finished

The ITCH message types, read and decoding logic, and parser are done. They've been unit tested and fuzzed. The counts for the test day are below.

message                               count
A  AddOrder                       117145568
F  AddOrderWithMPID                 1485888
E  ExecuteOrder                     5722824
C  ExecuteOrderWithPrice              99917
X  CancelOrder                      2787676
D  DeleteOrder                    114360997
U  ReplaceOrder                    21639067
   NotSupported                     5502843
total                             268744780

8251.4 MB in 3.920 s: 68.6 M msg/s, 2105 MB/s

<!-- One entry per experiment. Write the hypothesis and the prediction before running it.
Wrong guesses stay in.

## Template

### YYYY-MM-DD: <experiment>

- **Hypothesis:**
- **Prediction (before running):**
- **Setup:** commit, preset, results directory
- **Measured:**
- **Conclusion:** -->
