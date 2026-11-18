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

The ITCH message types, read and decoding logic, and parser are done. They've been unit tested and fuzzed. The counts for the test day are below (file `data/12302019.NASDAQ_ITCH50`).

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

## 2026-11-16 Stage 2: Order book

The goal of this stage is to get the order book working, and make it performant (low latency, tight latency distribution). In detail, here are the order of goals:
- Make the naive, simple, and natural algorithm to serve as a baseline 
- Write unit tests and confirm its behaviour is correct
- Make sure it's correct under ASAN, UBSAN
- Count anytimes a reference is missing.
- Write tooling to report latency per operation, as well as percentiles (p50, p99, p99.9, max) all per message type.
- Write baseline latency test (for example consuming messages with a nop). This is our latency floor, and tests timer overhead on the per-message latency
- Once all this is in place, we can test different orderbook implementations on ITCH data. 

### Order Book Spec 

Below is the SPEC for the order book.

Rough Implementation Spec:

- Two ordered sequences
  - Bids: Highest to lowest price
  - Asks: Lowest to highest price
- Price Level:
  - Price
  - Size: sum of all orders' size at this price level
- Each order has an ID (uint64_t) which is unique through the trading day
- We are assuming an aggregate book. 
- There is an order book per stock. Each is indexed by the stock locate of the order. 

Client API:

Note: all of these work on the specific stock locate given

1. add(order, locate, side, price, shares): From A and F
- Assumes: order isn't already in the book, and shares > 0
- Effect: creates a new entry in specified side, updating or creating a new price level with the amount of shares. Keeps track of orders' shares

2. execute(order, locate, shares): From E and C
- Assumes order exists, 0 < shares <= remaining order shares
- Effect: Updates the orders' price level to subtract the amount of shares on the order's side, deleting it if it reaches 0. Also subtracts shares from that specific order, deleting if it reaches 0 as well. 

4. cancel(order, locate, shares): from X
- Assumes: the order exists, 0 < shares <= remaining order shares
- Effect: Same as above

5. remove(order, locate): from D
- Assumes: the order exists
- Effect: order is removed from the order table, on the order's side, and all shares are subtracted from its corresponding price level. The price level is removed if it goes to 0. 

6. replace(oldOrder, newOrder, locate, price, shares): from U
- Assumes: oldOrder exists, newOrder doesn't, and 0 < shares
- Effect: removes oldOrder from its side, then adds the newOrder with specified price and shares on the old order's side

Queries:

7. best(locate, side):
  - for side == bid: returns highest price and its total quantity (or empty) 
  - for side == ask: returns lowest price and its total quantity (or empty)
  - if a best price doesn't exist, returns nullopt

8. quantityAt(locate, side, price): returns the total shares at the price, on the specified side 
9. priceLevelCount(locate, side): returns the total number of price levels on a side
10. contains(order): returns true if the order exists


### Naive Implementation Baseline

To effectively test the latency of different implementations, we need 2 controls: a latency ceiling (reference baseline), and a latency floor. For our reference baseline, we want to use a simple, natural, but relatively efficient version.

For this, we will use:
- Each stock maps to two std::map for both asks and bids, through indexing in an std::vector
- The bids and asks are std::maps indexing prices to volume
- std::unordered_map to map order IDs to side, price, and remaining shares

### Done When

- Naive order book is written with expects on preconditions, otherwise we ignore. It should be passing unit tests, ASAN and UBSAN and fuzzing with no exceptions.
- Latency reference baseline is written for timer/parser overhead
- We can view the per-operation latency histogram per message type, as well as percentiles (p50, p99, p99.9, max) without exceptions
- This is all implemented in a way that supports easy addition of new order book implementations for benchmarking, and gathering percentile data

## 2026-11-17 Order Book Reference Impl Done

The reference order book implementation is done, at `book/map_book.h`. It is meant as a latency baseline reference for the rest of implementations to compare against. As a precursory look, here are the numbers reported through the `replay.cpp` function, passing through the parser as well as the order book (file `data/12302019.NASDAQ_ITCH50`).

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

8251.4 MB in 199.890 s: 1.3 M msg/s, 41 MB/s

As another quick reference I tried running the MapBook with a pool allocator (`std::pmr::unsynchronized_pool_resource`), and it didn't speed anything up significantly

8251.4 MB in 196.049 s: 1.4 M msg/s, 42 MB/s

<!-- One entry per experiment. Write the hypothesis and the prediction before running it.
Wrong guesses stay in.

## Template

### YYYY-MM-DD: <experiment>

- **Hypothesis:**
- **Prediction (before running):**
- **Setup:** commit, preset, results directory
- **Measured:**
- **Conclusion:** -->
