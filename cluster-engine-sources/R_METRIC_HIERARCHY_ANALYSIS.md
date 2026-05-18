# R-METRIC-HIERARCHY RULE Analysis: Expression and Constraint Passing

## Overview

The `R-metric-hierarchy` rules enforce that musical event onsets (starting points of notes and rests) align with allowed positions in the metric grid (time signature structure). This document traces how these rules are expressed at the user level and propagated through the constraint solver architecture.

---

## Logical Expression of the Problem

### Constraint Formulation

The r-metric-hierarchy constraint can be expressed as a **satisfaction problem** over two synchronized engines:

```
GIVEN:
  - RhythmEngine:     Sequence of durations D = [d₁, d₂, ..., dₙ]
                      Onsets O = [o₁, o₂, ..., oₙ] where oᵢ = Σⱼ₌₁ᶦ dⱼ
  - MetricEngine:     Time signature sequence T = [t₁, t₂, ..., tₘ]
                      Beat grid G = [g₁, g₂, ..., gₖ] (allowed positions)

CONSTRAINT:
  ∀ oᵢ ∈ O (where i ∈ {1..n}):
    ∃ gⱼ ∈ G such that:
      offset(oᵢ, gⱼ) = 0

  WHERE:
    offset(onset, grid_point) = onset - grid_point

INTERPRETATION:
  Every rhythm event onset must align exactly with a position in the metric grid.
  No tolerance is permitted (offset must be precisely zero).

FAILURE CONDITION:
  If ∃ oᵢ ∈ O and ∀ gⱼ ∈ G: offset(oᵢ, gⱼ) ≠ 0
  Then the constraint is violated.

BACKTRACKING DECISION:
  On violation, choose to backtrack:
    - MetricEngine (if *bktr-rmh2v* = 3):  Try different time signatures/grid
    - RhythmEngine (if *bktr-rmh2v* = 4):  Try different duration sequence

```

### Formal Definition

**Strict Constraint** (true/false):

```
Satisfied(RhythmEngine, MetricEngine) ⟺
  ∀ onset ∈ Onsets(RhythmEngine):
    |offset_to_nearest_grid_point(onset, MetricEngine)| = 0
```

**Heuristic Constraint** (weight-based):

```
Weight(onset, MetricEngine) =
  COUNT(onsets_with_offset = 0) / TOTAL(onsets)

  - Weight = 1.0 ⟹ All onsets aligned (best)
  - Weight = 0.0 ⟹ No onsets aligned (worst)
  - Used for candidate sorting, never causes failure
```

### Modes of Operation

| Mode               | Scope                                          | Behavior                                  |
| ------------------ | ---------------------------------------------- | ----------------------------------------- |
| **:durations**     | Grace notes included, event endpoints excluded | Checks only start points of durations     |
| **:include-rests** | All events including rests, endpoints included | Checks start and end points of all events |

### Example

```
Input:
  RhythmEngine: durations = [1/4, 1/4, 1/2]
                onsets = [0, 1/4, 1/2, 1]
  MetricEngine: time_sig = 4/4
                grid = [0, 1/4, 1/2, 3/4, 1]  (sixteenth positions)

Analysis:
  onset 0   → grid_point 0   → offset = 0   ✓ PASS
  onset 1/4 → grid_point 1/4 → offset = 0   ✓ PASS
  onset 1/2 → grid_point 1/2 → offset = 0   ✓ PASS
  onset 1   → grid_point 1   → offset = 0   ✓ PASS

Result: Constraint SATISFIED

Counterexample:
  onset = 0.27 (not on grid)
  nearest grid_points = [0.25, 0.5]
  offset to 0.25 = 0.02  ✗ FAIL
  → Constraint violated → BACKTRACK
```

---

## 1. USER-LEVEL INTERFACE

### Entry Point: `R-metric-hierarchy`

**File**: `_000.main-interface.lisp` (Line 2853)

```lisp
(defun R-metric-hierarchy (voices rule-mode &optional rule-type weight)
  "
  Rule for metric hierarchy: the onsets of events will be forced to line
  up to the metric grid.

  <voices>: number or list of voice numbers (0-based)

  <rule-mode>:
    - :durations  (grace notes included, endpoints excluded)
    - :include-rests (all events with endpoints included)

  <rule-type> (optional):
    - :true/false (strict constraint - default)
    - :heur-switch (heuristic preference with weight)

  <weight>: numeric preference value (for heuristic mode)
  "
```

### Key Design Decisions at User Level:

1. **Voice Selection**: Can apply to single voice or list of voices
2. **Mode Selection**: Different strictness levels (durations vs. include-rests)
3. **Rule Type**: Can be strict constraint or soft heuristic guidance
4. **Backtracking Preference**: Global flag `*bktr-rmh2v*` controls which engine backs up on failure

---

## 2. RULE EXPRESSION LAYER

### Processing Pipeline in R-metric-hierarchy

```lisp
(when (typep voices 'number) (setf voices (list voices)))
(let ((rhythm-engines (mapcar #'(lambda (voice) (* 2 voice)) voices)))
  (cond ((equal rule-type :heur-switch)
         ;; HEURISTIC MODE
         (cond ((equal rule-mode :durations)
                (loop for rhythm-engine in rhythm-engines
                      collect (heuristic-rule-two-engines
                        (heuristic-switch-rule-2-engines-metric-grid-rhythm-hierarchy
                          rhythm-engine weight) -1 rhythm-engine)))
               ((equal rule-mode :include-rests)
                (loop for rhythm-engine in rhythm-engines
                      collect (heuristic-rule-two-engines
                        (heuristic-switch-rule-2-engines-metric-grid-rhythm-hierarchy-include-rests
                          rhythm-engine weight) -1 rhythm-engine)))))
        (t
         ;; STRICT MODE
         (cond ((equal rule-mode :durations)
                (cond ((= *bktr-rmh2v* 3)
                       ;Prefer to backtrack metric engine
                       (loop for rhythm-engine in rhythm-engines
                             collect (rule-two-engines3
                               (rule-2-engines-metric-grid-rhythm-hierarchy rhythm-engine)
                               -1 rhythm-engine)))
                      ((= *bktr-rmh2v* 4)
                       ;Prefer to backtrack rhythm engine
                       (loop for rhythm-engine in rhythm-engines
                             collect (rule-two-engines4
                               (rule-2-engines-metric-grid-rhythm-hierarchy rhythm-engine)
                               -1 rhythm-engine)))))))))
```

### Key Observations:

1. **Engine Mapping**: Voice number N → rhythm engine = 2N, pitch engine = 2N+1
2. **Metric Engine Flag**: -1 is a placeholder replaced with actual metric engine number later
3. **Multiple Variants**: Separate implementations for different modes and heuristic vs. strict
4. **Backtrack Control**: Two wrapper functions (`rule-two-engines3` vs. `rule-two-engines4`) determine which engine is preferred for backtracking

---

## 3. CONSTRAINT GENERATION LAYER

### Strict Constraint: `rule-2-engines-metric-grid-rhythm-hierarchy`

**File**: `05d.rules-interface-2engines.lisp` (Line 317)

```lisp
(defun rule-2-engines-metric-grid-rhythm-hierarchy (rhythm-engine1)
  "
  Formats a rule for metric hierarchy.
  How durations (including gracenotes) are positioned within the metric structure.
  Allowed positions defined in the metric domain.
  "

  ;; LAMBDA FUNCTION RETURNED (the actual constraint)
  (list 'lambda '(vsolution vlinear-solution vindex
                  vsolution-for-backjump vbackjump-indexes engine)

        ;; INITIALIZATION
        '(declare (type array vsolution vlinear-solution vindex
                         vsolution-for-backjump vbackjump-indexes))

        ;; CHECK LOGIC: Two cases - when rhythm-engine is being populated
        (list 'block 'this-rule
              (list 'let '((metric-engine2 (1- (the fixnum
                            (array-dimension vindex 0))))
                           list-of-offsets
                           timepoints-for-backjump)

                    ;; CASE 1: Check when rhythm engine has new values
                    (list 'cond (list (list '= 'engine rhythm-engine1)
                                      ;; Extract metric grid points
                                      (list 'let*
                                        ((list 'this-cell-onsets-plus-preceding-for-extra-args
                                               (list 'butlast
                                                 (list 'get-timepoints-from-start-last-rhythmcell-minus-nsteps-ignor-rests-simplify-gracenotes
                                                       rhythm-engine1 'vindex 'vsolution 'vlinear-solution 0)))
                                         (list 'onsetgrid-metric-engine2
                                               (list 'aref 'vlinear-solution 'metric-engine2 2))
                                         (list 'matching-or-preceding-timepoints-engine1
                                               (list 'find-all-timepoints-convert-rests
                                                     'engine1-timepoints-to-check
                                                     'onsetgrid-metric-engine2)))

                                        ;; Calculate offsets from metric grid
                                        (list 'setf 'list-of-offsets
                                              '(mapcar '- 'matching-or-preceding-timepoints-engine1
                                                       'engine1-timepoints-to-check)))

                                      ;; TEST: All offsets must be zero
                                      (list 'loop 'for 'nth-variable 'from 0
                                            'do (list 'when (list 'not
                                                        '(= (nth nth-variable list-of-offsets) 0))
                                                      ;; BACKJUMP on failure
                                                      (list 'set-vbackjump-indexes-from-failed-timepoint-duration
                                                            ...)))
                                            'finally '(return t))))

                    ;; CASE 2: Similar for when metric engine has new values
                    (list (list '= 'engine 'metric-engine2)
                          ...))))
```

### Heuristic Variant: `heuristic-switch-rule-2-engines-metric-grid-rhythm-hierarchy`

**File**: `06d.heuristic-rules-interface-2engines.lisp` (Line 361)

```lisp
(defun heuristic-switch-rule-2-engines-metric-grid-rhythm-hierarchy
    (rhythm-engine1 weight)
  "
  Heuristic version: returns weight instead of true/false.
  Weights sort candidates locally before strict rules are applied.
  "

  (list 'lambda '(vsolution vlinear-solution vindex engine nth-candidate)

        ;; Similar structure to strict version, but:
        ;; - Returns 0 instead of (return-from this-rule t) on failure
        ;; - Final test returns weight average instead of t/nil

        (list 'average
              (list 'loop 'for 'nth-variable 'from 0
                    'collect (list 'if
                              '(= (the number (nth nth-variable list-of-offsets)) 0)
                              weight
                              0)))))
```

### Rule Wrapping Functions

#### Strict Rules: `rule-two-engines3` and `rule-two-engines4`

**File**: `05b.rules-interface-2engines.lisp` (Line 880)

```lisp
(defun rule-two-engines3 (rule engine1 engine2)
  "Prefers backtracking in engine1"
  (let ((compiled-rule (compile-if-not-compiled nil rule))
        (vrule (make-array '(3))))

    ;; Element 0: Engines where this rule applies
    (setf (aref vrule 0) (list engine1 engine2))

    ;; Element 1: Compiled constraint function
    (setf (aref vrule 1) compiled-rule)

    ;; Element 2: Backtrack routes on failure
    ;; If rule fails, prefer to backtrack engine1 first, then engine1 again
    (setf (aref vrule 2) (list (list engine1 engine2)
                               (list engine1 engine2)))

    ;; Wrap in rule instance object
    (make-rule-instance vrule)))

(defun rule-two-engines4 (rule engine1 engine2)
  "Prefers backtracking in engine2"
  (let ((compiled-rule (compile-if-not-compiled nil rule))
        (vrule (make-array '(3))))

    (setf (aref vrule 0) (list engine1 engine2))
    (setf (aref vrule 1) compiled-rule)

    ;; If rule fails, prefer to backtrack engine2 first, then engine2 again
    (setf (aref vrule 2) (list (list engine2 engine1)
                               (list engine2 engine1)))

    (make-rule-instance vrule)))
```

#### Heuristic Rules: `heuristic-rule-two-engines`

**File**: `06b.heuristic-rules-interface-2engines.lisp` (Line 1262)

```lisp
(defun heuristic-rule-two-engines (rule engine1 engine2)
  "Wraps heuristic rule (no backtrack routes)"
  (let ((compiled-rule (compile-if-not-compiled nil rule))
        (vrule (make-array '(2))))  ;; Only 2 elements (no backtrack)

    ;; Element 0: Engines where this rule applies
    (setf (aref vrule 0) (list engine1 engine2))

    ;; Element 1: Compiled heuristic function
    (setf (aref vrule 1) compiled-rule)

    (make-heuristic-rule-instance vrule)))
```

---

## 4. CONSTRAINT OBJECT LAYER

### Rule Instance Classes

**File**: `05.rules-interface.lisp`

```lisp
(defclass rule ()
  ((layer :type array :initform (make-array 1)
          :reader get-rule :writer set-rule)))

(defclass heuristic-rule ()
  ((layer :type array :initform (make-array 1)
          :reader get-heuristic-rule :writer set-heuristic-rule)))
```

### Instance Creation

```lisp
(defun make-rule-instance (rule-vector)
  "Wrap rule in object for type dispatch"
  (let ((this-instance (make-instance 'rule)))
    (set-rule rule-vector this-instance)
    this-instance))

(defun make-heuristic-rule-instance (hrule-vector)
  "Wrap heuristic rule in object for type dispatch"
  (let ((this-instance (make-instance 'heuristic-rule)))
    (set-heuristic-rule hrule-vector this-instance)
    this-instance))
```

### Internal Structure of Rule Vector

**Strict Rule (array of 3 elements)**:

```
[0] Engines:         (list rhythm-engine metric-engine)
[1] Function:        Compiled lambda that tests constraint
[2] Backtrack Info:  (list (list engine1 engine2)
                           (list engine1 engine2))
```

**Heuristic Rule (array of 2 elements)**:

```
[0] Engines:    (list rhythm-engine metric-engine)
[1] Function:   Compiled lambda that returns weight
```

---

## 5. CONSTRAINT PASSING TO ENGINE

### Rule Vector Creation: `create-rule-vector`

**File**: `05.rules-interface.lisp`

```lisp
(defun create-rule-vector (rules-input-list nr-of-engines locked-engines)
  "
  Converts high-level rule objects into a 2D constraint array.

  Output structure:
    vrules[engine_number][rule_index] = constraint_function
  "

  ;; Extract rule objects from mixed input list
  (let ((rules (filter-rules-from-input rules-input-list)))

    ;; Replace metric engine -1 flags with actual metric engine number
    (replace-flags-with-metric-engine-in-rules rules nr-of-engines)

    ;; Remove locked engines from backtrack routes
    (loop for rule in rules
          do (remove-locked-engines-in-backtrackroutes rule locked-engines))

    ;; Build 2D array indexed by [engine][rule]
    ;; Array structure allows fast lookup of all rules for a given engine
    (setf vrules (create-array-for-rules ...))

    ;; Populate array
    (loop for rule in rules
          do (loop for engine in (aref rule 0)
                   do (setf (aref vrules engine ...) rule)))

    vrules))
```

---

## 6. CONSTRAINT TESTING IN SOLVER LOOP

### Rule Execution: `test-rules`

**File**: `05.rules-interface.lisp` (Line 188)

```lisp
(defun test-rules (engine vrules vsolution vlinear-solution
                   vsolution-for-backjump vbackjump-indexes vindex vbacktrack-engines)

  (if (aref vrules engine 0) ;; If rules exist for this engine

      ;; Test all rules
      (let ((ruletest (remove t
              (loop for ruleindex from 0 to (1- (array-dimension (aref vrules engine 0) 0))
                    collect (if (funcall (aref (aref vrules engine 0) ruleindex)
                                         vsolution vlinear-solution vindex
                                         vsolution-for-backjump vbackjump-indexes
                                         engine)
                                t                    ;; Rule passed
                              (aref (aref vrules engine 1) ruleindex))))))

        ;; If any rule failed, ruletest contains backtrack route
        (if ruletest
            (progn
              ;; CONSTRAINT VIOLATION: Set backtrack info
              (setf (aref vbacktrack-engines 0)
                    (append ruletest (aref vbacktrack-engines 0)))
              (return-from test-rules nil))

            ;; ALL CONSTRAINTS SATISFIED
            (progn
              (setf (aref vbacktrack-engines 0) nil)
              (return-from test-rules t))))

      ;; No rules to check
      t))
```

### Execution Flow

```
Main Search Loop
    ↓
Candidate Selected for Engine
    ↓
Convert vsolution to vlinear-solution (flattened representation)
    ↓
Call (test-rules engine vrules ...)
    ↓
For each rule in vrules[engine]:
    ├─ Call rule lambda function with:
    │  ├─ vsolution: full search path (nested arrays)
    │  ├─ vlinear-solution: flattened current solution
    │  ├─ vindex: current search indexes
    │  ├─ vsolution-for-backjump: onset/count data for backjumping
    │  ├─ vbackjump-indexes: indexes to jump to
    │  └─ engine: current engine number
    ├─ If returns T: constraint satisfied, continue
    └─ If returns (list engines): backtrack these engines
    ↓
If all constraints pass: Call forward rule (select next engine)
If constraint fails: Call backtrack rule (which engine to undo)
```

---

## 7. CONSTRAINT LOGIC IN DETAIL

### What the Metric Hierarchy Constraint Actually Tests

The constraint checks that event onsets from the rhythm engine align with the metric grid:

```
metric-engine contains:
  [0] Time signatures (e.g., (4 4), (3 4))
  [1] Beat structure  (e.g., major beats within measure)
  [2] Onset grid      (e.g., allowed beat positions)

rhythm-engine contains:
  [0] Durations
  [1] Onsets (offsets between consecutive durations)
  [2] Note counts

Constraint Test:
  For each onset in rhythm-engine:
    Find nearest position in metric-engine's onset-grid
    Calculate offset = (actual-onset - grid-position)
    Require: offset == 0

  If any offset != 0:
    Constraint fails
    Backtrack preferred engine (rhythm or metric based on *bktr-rmh2v*)
```

### Offset Calculation Code

```lisp
;; Extract onsets from rhythm engine
(setf engine1-timepoints-to-check
      (filter-timepoints-keep-upto-endtime
        this-cell-onsets-plus-preceding-for-extra-args
        end-time-metric-engine2))

;; Find matching metric grid positions
(setf matching-or-preceding-timepoints-engine1
      (find-all-timepoints-convert-rests
        engine1-timepoints-to-check
        onsetgrid-metric-engine2))

;; Calculate offsets
(setf list-of-offsets
      (mapcar '- matching-or-preceding-timepoints-engine1
              engine1-timepoints-to-check))

;; Test: all must be zero
(loop for nth-variable from 0
      do (when (not (= (the number (nth nth-variable list-of-offsets)) 0))
           (return nil)))  ;; Constraint fails
```

---

## 8. CONFIGURATION AND CONTROL

### Backtrack Preference Global Variable

**File**: `05.rules-interface.lisp` (Line 59)

```lisp
(defvar *bktr-rmh2v* 4)
;; Flag for rhythm-metric-hierarchy-rules
;; - 3: Prefer to backtrack metric engine on failure
;; - 4: Prefer to backtrack rhythm engine on failure (default)
```

### Set During Initialization

**File**: `_000.main-interface.lisp` (Line 334)

```lisp
(setf *bktr-rmh2v* (cond ((equal bktr-rmh2v :meter) 3)
                          ((equal bktr-rmh2v :rhythm) 4)))
```

---

## 9. COMPLETE CONSTRAINT PIPELINE SUMMARY

```
USER LEVEL:           R-metric-hierarchy(voice, mode, type, weight)
                                  ↓
RULE CREATION:        Creates lambda functions for constraint checking
                      (rule-2-engines-metric-grid-rhythm-hierarchy)
                                  ↓
RULE WRAPPING:        Wraps in backtrack information structure
                      (rule-two-engines3 or rule-two-engines4)
                                  ↓
INSTANCE CREATION:    Creates rule/heuristic-rule objects
                      (make-rule-instance)
                                  ↓
VECTOR CONSTRUCTION:  Creates 2D array vrules[engine][rule]
                      (create-rule-vector)
                                  ↓
ENGINE SETUP:         Replaces metric-engine -1 flags with actual numbers
                      (replace-flags-with-metric-engine-in-rules)
                                  ↓
SEARCH LOOP:          For each candidate:
                      (test-rules engine vrules ...)
                      - Call each constraint function
                      - Collect failures
                      - Return backtrack routes or success
                                  ↓
BACKTRACK:            If constraint fails, backtrack preferred engine
                      Determined by *bktr-rmh2v* flag
```

---

## 10. KEY FILES AND LOCATIONS

| Component                  | File                                          | Lines     |
| -------------------------- | --------------------------------------------- | --------- |
| User Interface             | `_000.main-interface.lisp`                    | 2853-2920 |
| Strict Constraint Logic    | `05d.rules-interface-2engines.lisp`           | 317-450   |
| Heuristic Constraint Logic | `06d.heuristic-rules-interface-2engines.lisp` | 361-480   |
| Rule Wrapping              | `05b.rules-interface-2engines.lisp`           | 880-920   |
| Rule Classes               | `05.rules-interface.lisp`                     | 1-100     |
| Heuristic Classes          | `06.heuristic-rules-interface.lisp`           | 1-20      |
| Rule Vector Creation       | `05.rules-interface.lisp`                     | 116-200   |
| Constraint Testing         | `05.rules-interface.lisp`                     | 188-280   |
| Engine Main Loop           | `02.engine.lisp`                              | 1-200+    |
| Backtrack Configuration    | `05.rules-interface.lisp`                     | 59        |

---

## 11. KEY CONCEPTS

### Strict vs. Heuristic Rules

**Strict Rules**:

- Return `T` (pass) or backtrack routes (fail)
- Can cause search failure if violated
- Hard constraints on composition

**Heuristic Rules**:

- Return numeric weight (0-1 range typically)
- Sort candidates by weight locally
- Never cause failure
- Soft preferences for musical style

### Dual-Engine Nature

Metric hierarchy involves **two engines**:

1. **Rhythm Engine** (even number): Contains durations and their onsets
2. **Metric Engine** (last engine): Contains time signatures and allowed beat positions

The constraint synchronizes these two engines' timing.

### Modes of Operation

- **:durations**: Grace notes included, event endpoints excluded from check
- **:include-rests**: All events including rests, endpoints included

### Backtracking Strategies

- **engine1 preferred** (`rule-two-engines3`): If constraint fails, try different rhythm values first
- **engine2 preferred** (`rule-two-engines4`): If constraint fails, try different metric values first

Both can be chosen via `*bktr-rmh2v*` global variable.

---

## Example: Complete Constraint Instantiation

```lisp
;; User creates:
(R-metric-hierarchy 0 :durations :true/false)

;; Which generates:
(heuristic-rule-two-engines
  (heuristic-switch-rule-2-engines-metric-grid-rhythm-hierarchy 0 1)
  -1 0)

;; Or in strict mode:
(rule-two-engines4
  (rule-2-engines-metric-grid-rhythm-hierarchy 0)
  -1 0)

;; Creates objects with internal structure:
;; Strict rule vector:
;;   [0] (0 -1)  ;; engines (rhythm=0, metric=-1/replaced)
;;   [1] #<compiled-function>
;;   [2] ((0 -1) (0 -1))  ;; backtrack routes

;; Combined with other rules into:
vrules = 2D-ARRAY[nr-of-engines][nr-of-rules-per-engine]

;; During search, engine tests:
(test-rules 0 vrules vsolution vlinear-solution ...)
 ;; Which calls:
  (funcall (aref (aref vrules 0 0) 0)
           vsolution vlinear-solution vindex ...)
 ;; Lambda checks: Do all rhythm onsets match metric grid?
```
