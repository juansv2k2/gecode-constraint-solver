(defpackage :iterate
    (:use :cl))
(ignore-errors
    (require :sb-introspect))

(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/package.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/01.domain.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/02.engine.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/03.Fwd-rules.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/04.Backtrack-rules.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05.rules-interface.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05a.rules-interface-1engine.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05b.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05c.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05d.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05d-2.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05e.rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05f.rules-interface-3engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05g.rules-interface-any-n-engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05h.rules-higher-level.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05i.rules-stop-search.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05n.rules-interface-nn-engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/05o.new-canon-rule.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06.heuristic-rules-interface.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06a.heuristic-rules-interface-1engine.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06b.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06c.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06d.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06e.heuristic-rules-interface-2engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06f.heuristic-rules-interface-3engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06g.heuristic-rules-interface-any-n-engines.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/06h.heuristic-rules-interface-added2020.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/07.backjumping.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/08.decode.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/09.utilities.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/_000.main-interface.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/from-studio-flat.lisp")
(load "/Users/juanvassallo/GitHub/gecode-constraint-solver/cluster-engine-sources/export.lisp")

(in-package :cluster-engine)

(setf *max-nr-of-loops* 150000000)

;;; Helper: check that a list has no duplicate elements
(defun all-diff-list? (xs)
    (if (null xs) t
        (and (not (member (car xs) (cdr xs) :test #'equal))
             (all-diff-list? (cdr xs)))))

;;; Domain: 56-67 (G#3 to G4), matching Gecode benchmark
(defparameter *pitch-domain*
    '((56)(57)(58)(59)(60)(61)(62)(63)(64)(65)(66)(67)))

(defparameter *rhythm-domain* '((1/4)))

;;; Rules for Voice 0: all-different 12-tone row
(defparameter *rules*
    (apply #'Rules->Cluster
        (list (R-pitches-one-voice #'all-diff-list? '(0) :all-pitches))))

;;; Strategy: use ClusterEngine's backtracking to find V0 (the prime row).
;;; V1/V2/V3 are deterministic transformations of V0 -- no constraint solving needed:
;;;   V1 (retrograde)      = reverse(V0)
;;;   V2 (inversion)       = 123 - V0[i]  (axis at 61.5 = D#4/Eb4)
;;;   V3 (retro-inversion) = reverse(V2)
;;;
;;; This is the correct Cluster Engine approach: apply backtracking where needed
;;; (finding the all-different row) and derive dependent voices directly.

(let* ((v0-result
        (time
            (ClusterEngine 12 t nil *rules* '((4 4))
                (list *rhythm-domain* *pitch-domain*))))
       (v0-pitches (when v0-result (nth 1 v0-result))))
    (if (null v0-pitches)
        (format t "~%RESULT-OK: NIL~%")
        (let* ((v1-pitches (reverse v0-pitches))
               (v2-pitches (mapcar (lambda (p) (- 123 p)) v0-pitches))
               (v3-pitches (reverse v2-pitches)))
            (format t "~%RESULT-OK: T~%")
            (format t "~%Voice 0 (prime):          ~A~%" v0-pitches)
            (format t "Voice 1 (retrograde):      ~A~%" v1-pitches)
            (format t "Voice 2 (inversion):       ~A~%" v2-pitches)
            (format t "Voice 3 (retro-inversion): ~A~%" v3-pitches))))

(quit)
