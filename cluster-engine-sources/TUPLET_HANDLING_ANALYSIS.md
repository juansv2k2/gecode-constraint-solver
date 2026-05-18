# Tuplet (Quintuplet, Sextuplet, etc.) Handling in Cluster Engine

## Overview

The Cluster Engine supports arbitrary tuplet subdivisions of beats through a sophisticated beat grid generation system. Tuplets like quintuplets (1/5), sextuplets (1/6), septuplets (1/7), and many others are handled uniformly through a **tuplet factor multiplication mechanism** that subdivides each beat into equal divisions.

---

## Core Concept: Tuplet Division Formula

### Mathematical Foundation

A tuplet is specified by a single integer N that represents how many equal parts the beat is divided into:

```
Tuplet Division Formula:
  interval_duration = (beat_duration / N)
  
  For a beat of length 1/4 (quarter note):
    - Triplet (N=3):     interval = (1/4) / 3 = 1/12
    - Quintuplet (N=5):  interval = (1/4) / 5 = 1/20
    - Sextuplet (N=6):   interval = (1/4) / 6 = 1/24
    - Septuplet (N=7):   interval = (1/4) / 7 = 1/28
    - Nontuplet (N=9):   interval = (1/4) / 9 = 1/36
    
  Total positions in measure = N * beats_per_measure
  
  Example with 4/4 time and triplets (N=3):
    Total positions = 3 * 4 = 12 positions per measure
    Each position = 1/12
    Grid = [0, 1/12, 2/12, 3/12, 4/12, 5/12, 6/12, 7/12, 8/12, 9/12, 10/12, 11/12, 1]
```

---

## Implementation in Source Code

### User-Level Input

**File**: `01.domain.lisp`, `_000.main-interface.lisp`

```lisp
(create-metric-domain-vector timesign-list tuplets-list alt-beatlength-list)

;; Example:
(create-metric-domain-vector '((4 4))        ;; time signature: 4/4
                             '((3 4 5))      ;; tuplets: triplets, quadruplets, quintuplets
                             '(nil))         ;; no alternative beat length
```

### Tuplet List Interpretation

The tuplets input is a **list of integers**, each representing:

```lisp
'(3 4 5 6 7 8 9 12)    ;; Allowed tuplet divisions of the beat

Meaning:
  3  = Triplet      (3 notes per beat)
  4  = Quadruplet   (4 notes per beat)  
  5  = Quintuplet   (5 notes per beat)
  6  = Sextuplet    (6 notes per beat)
  7  = Septuplet    (7 notes per beat)
  8  = Octuplet     (8 notes per beat)
  9  = Nontuplet    (9 notes per beat)
  12 = Duodecuplet  (12 notes per beat)
```

---

## Grid Generation Process

### Step 1: Onset Grid Calculation

**File**: `01.domain.lisp` (Line 37-56)

```lisp
(defun make-onset-grid2 (tuplets timesign alt-beatlength)
  "Generate all allowed beat positions (onset grid) for a time signature
   given a set of tuplet subdivisions."
  
  (cond ((not alt-beatlength)
         ;; Default beat length from time signature
         ;; For each tuplet N in the list:
         (remove-duplicates 
           (sort (apply 'append 
             (loop for tuplet in tuplets
                   collect (dx-to-x 0 
                     (make-list (* (first timesign) tuplet)
                                :initial-element 
                                (/ (/ 1 (second timesign)) tuplet)))))
           '<))))
```

### Breakdown of Calculation

For a 4/4 time signature with tuplets '(3 4 5):

```
Beat length = 1/4 (second element of time sig is denominator)

For each tuplet N:
  Number of subdivisions in measure = (first timesign) * N
                                    = 4 * N
  
  Interval between positions = (beat_length / N)
                              = (1/4) / N
  
TRIPLET (N=3):
  Subdivisions = 4 * 3 = 12
  Interval = (1/4) / 3 = 1/12
  Positions = [0, 1/12, 2/12, 3/12, 4/12, 5/12, 6/12, 7/12, 8/12, 9/12, 10/12, 11/12, 1]

QUINTUPLET (N=5):
  Subdivisions = 4 * 5 = 20
  Interval = (1/4) / 5 = 1/20
  Positions = [0, 1/20, 2/20, 3/20, 4/20, ..., 19/20, 1]

QUADRUPLET (N=4):
  Subdivisions = 4 * 4 = 16
  Interval = (1/4) / 4 = 1/16
  Positions = [0, 1/16, 2/16, 3/16, ..., 15/16, 1]

COMBINED GRID (remove duplicates and sort):
  Merge all positions from all tuplets
  Result = [0, 1/20, 1/16, 1/12, 2/20, 3/20, 1/5, 1/4, ...]
```

### Step 2: List Conversion

**File**: `09.utilities.lisp` (Line 534)

```lisp
(defun dx-to-x (start list)
  "Convert deltas (differences) to absolute positions.
   This cumulative sum creates the actual grid points."
  
  (let ((n start))
    (cons start 
      (loop for x in list
            collect (setf n (+ n x))))))

;; Example:
;; Input:  (dx-to-x 0 '(1/12 1/12 1/12 1/12))
;; Output: (0 1/12 2/12 3/12 4/12)
```

### Step 3: Deduplication

```lisp
(remove-duplicates (sort ... '<))

;; This is crucial because:
;; - Triplets create positions at 1/12, 2/12, 3/12, 4/12, etc.
;; - Quadruplets create positions at 1/16, 2/16, 3/16, 4/16, etc.
;; - Some positions coincide: 3/12 = 4/16, so keep only one
;;
;; Result: Union of all tuplet grids
```

---

## Alternative Beat Length Handling

### With Custom Beat Length

**File**: `01.domain.lisp` (Line 47-54)

When `alt-beatlength` is a number (not nil or a list), the calculation adjusts:

```lisp
(defun make-onset-grid2 (tuplets timesign alt-beatlength)
  ((numberp alt-beatlength)
   (truncate-list-at-endpoint 
     (apply '/ timesign)
     (remove-duplicates 
       (sort (apply 'append 
         (append (loop for tuplet in tuplets
                       collect (dx-to-x 0 
                         (make-list (* (truncate (+ (/ (apply '/ timesign) 
                                                           alt-beatlength) 1/2)) 
                                       tuplet) 
                                    :initial-element 
                                    (/ alt-beatlength tuplet))))
                 (list (list (apply '/ timesign)))))
        '<)))))
```

### Example: Alternative Beat Length in 6/8

```
Time signature: (6 8)
Normal beat length: 1/8
Alternative beat length: 3/8 (dotted quarter)
Tuplets: (3 4)

Calculation:
  Default measure length = 6/8
  Alt beat length = 3/8
  Beats in measure = (6/8) / (3/8) = 2 beats
  
  Number of subdivisions = (truncate (+ (/ (6/8) (3/8)) 1/2)) * N
                         = (truncate (+ 2 0.5)) * N
                         = 2 * N
                         
  Interval = (3/8) / N
  
For N=3:
  Subdivisions = 2 * 3 = 6
  Interval = (3/8) / 3 = 1/8
  Positions = [0, 1/8, 2/8, 3/8, 4/8, 5/8, 6/8]
  
For N=4:
  Subdivisions = 2 * 4 = 8
  Interval = (3/8) / 4 = 3/32
  Positions = [0, 3/32, 6/32, 9/32, 12/32, 15/32, 18/32, 21/32, 6/8]
```

---

## Practical Examples

### Example 1: Simple Triplets in 4/4

```lisp
User input:
(metric-domain (4 4) (3) nil)

Internal processing:
(create-metric-domain-vector '((4 4)) '((3)) '(nil))

Calculation:
  Tuplet = 3
  Beat length = 1/4
  Interval per position = (1/4) / 3 = 1/12
  Subdivisions per measure = 4 * 3 = 12
  
Generated grid:
  [0, 1/12, 2/12, 3/12, 4/12, 5/12, 6/12, 7/12, 8/12, 9/12, 10/12, 11/12, 1]
  
Simplified:
  [0, 1/12, 1/6, 1/4, 1/3, 5/12, 1/2, 7/12, 2/3, 3/4, 5/6, 11/12, 1]
```

### Example 2: Mixed Tuplets (Triplets + Quintuplets) in 4/4

```lisp
User input:
(metric-domain (4 4) (3 5) nil)

Calculation:
  
  Triplets (N=3):
    Interval = 1/12
    Positions = [0, 1/12, 2/12, 3/12, 4/12, 5/12, 6/12, 7/12, 8/12, 9/12, 10/12, 11/12, 1]
  
  Quintuplets (N=5):
    Interval = 1/20
    Positions = [0, 1/20, 2/20, 3/20, 4/20, 5/20, 6/20, 7/20, 8/20, 9/20, 
                10/20, 11/20, 12/20, 13/20, 14/20, 15/20, 16/20, 17/20, 18/20, 19/20, 1]
  
  Merged (after removing duplicates):
    Grid includes positions from both:
    [0, 1/20, 1/12, 1/10, 1/5, 1/4, 3/10, 1/3, 4/10, 5/12, 1/2, ...]
    
  Total unique positions: ~29-32 positions per measure
```

### Example 3: All Common Tuplets in 4/4

```lisp
User input:
(create-metric-domain-vector '((4 4)) '((3 4 5 6 7 8 9 12)) '(nil))

Calculation:
  N=3:   Positions every 1/12      (12 positions)
  N=4:   Positions every 1/16      (16 positions)
  N=5:   Positions every 1/20      (20 positions)
  N=6:   Positions every 1/24      (24 positions)
  N=7:   Positions every 1/28      (28 positions)
  N=8:   Positions every 1/32      (32 positions)
  N=9:   Positions every 1/36      (36 positions)
  N=12:  Positions every 1/48      (48 positions)
  
  Combined unique grid: ~192 positions per measure (LCM-based calculation)
  
  LCM(12, 16, 20, 24, 28, 32, 36, 48) = 6,720
  Positions would be multiples of (1/6720)
```

---

## The Metric Hierarchy Constraint with Tuplets

When `R-metric-hierarchy` is used, the constraint checks:

```
For each rhythm event onset:
  Check if onset matches ANY position in the generated tuplet grid

Example:
  Grid with triplets: [0, 1/12, 2/12, 3/12, ..., 11/12, 1]
  
  Valid onsets: 0, 1/12, 1/6, 1/4, 1/3, 5/12, 1/2, 7/12, 2/3, 3/4, 5/6, 11/12, 1
  Invalid onset: 1/10 (NOT on triplet grid)
  
  If using both triplets and quintuplets:
  Additional valid: 1/20, 1/10, 3/20, 1/5, 7/20, 2/5, ...
```

---

## Key Implementation Details

### 1. Fractional Arithmetic

All calculations use **exact rational arithmetic** (Lisp rationals):

```lisp
(/ (/ 1 (second timesign)) tuplet)
```

This ensures no floating-point precision errors.

### 2. Grid Optimization

```lisp
(remove-duplicates (sort ... '<))
```

- **Sort**: Ensures ascending order
- **Remove-duplicates**: Merges overlapping positions from different tuplets
- **Result**: Minimal set of check positions

### 3. Truncation at Measure End

```lisp
(truncate-list-at-endpoint (apply '/ timesign) list)
```

Ensures the grid doesn't extend beyond the measure length.

### 4. Arbitrary Tuplet Support

The system is **completely general**:

```
Supported tuplets: ANY positive integer N
  N=1 : Whole beat (no subdivision)
  N=2 : Duple (2 notes per beat)
  N=3 : Triplet
  N=4 : Quadruplet
  N=5 : Quintuplet
  N=6 : Sextuplet
  N=7 : Septuplet
  N=8 : Octuplet
  N=9 : Nontuplet
  N=10: Decuplet
  N=11: Undecuplet
  N=12: Duodecuplet
  ... (any N)
```

---

## Configuration and Control

### Global Default

**File**: `14.back-compability.lisp` (Line 49)

```lisp
(when (not metric-domain) 
  (setf metric-domain (create-metric-domain-vector '((4 4)) 
                                                    '((5 6 7 8))  ;; Default tuplets
                                                    '(nil))))
```

Default tuplets: quintuplet, sextuplet, septuplet, octuplet

### User Override

```lisp
;; Use only triplets
(metric-domain (4 4) (3) nil)

;; Use triplets and tuplets up to 9-tuplets
(metric-domain (4 4) (3 4 5 6 7 8 9) nil)

;; Use only quintuplets
(metric-domain (4 4) (5) nil)
```

---

## Constraint Testing with Tuplets

**File**: `05b.rules-interface-2engines.lisp` (Line 903)

```lisp
(defun rule-two-engines-tuplets-in-meter (rule rhythm-engine)
  "Checks that rhythm onsets align with tuplet grid positions."
```

### Test Logic

```
1. Extract all onsets from rhythm engine candidate
2. For each onset O:
   a. Look up in metric engine's tuplet grid
   b. Find nearest grid point G
   c. Calculate offset = O - G
   d. If offset ≠ 0: CONSTRAINT FAILS
3. If all offsets = 0: CONSTRAINT PASSES
```

---

## Summary: How Tuplets Work

| Tuplet | Beat Subdivisions | Interval Size | Grid Density (4/4) |
|--------|------------------|---------------|--------------------|
| N=3 (triplet) | 3 | 1/12 | 12 positions |
| N=4 (quadruplet) | 4 | 1/16 | 16 positions |
| N=5 (quintuplet) | 5 | 1/20 | 20 positions |
| N=6 (sextuplet) | 6 | 1/24 | 24 positions |
| N=7 (septuplet) | 7 | 1/28 | 28 positions |
| N=8 (octuplet) | 8 | 1/32 | 32 positions |
| N=9 (nontuplet) | 9 | 1/36 | 36 positions |
| N=12 (duodecuplet) | 12 | 1/48 | 48 positions |

When multiple tuplets are specified, the grids are **merged** into a single unified grid containing all positions from all tuplets.

