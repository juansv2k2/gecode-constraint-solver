(defpackage :iterate (:use :cl))
(ignore-errors (require :sb-introspect))
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

(defun all-diff-list? (xs)
  (if (null xs) t
      (and (not (member (car xs) (cdr xs) :test #'equal))
           (all-diff-list? (cdr xs)))))

(defparameter *voices* '(0 1 2 3))
(defparameter *pitch-domain* '((48) (49) (50) (51) (52) (53) (54) (55) (56) (57) (58) (59) (60) (61) (62) (63) (64) (65) (66) (67) (68) (69) (70) (71) (72) (73) (74) (75) (76) (77) (78) (79) (80) (81) (82) (83)))
(defparameter *rhythm-domain* '((1/4)))
(defparameter *domains* (list *rhythm-domain* *pitch-domain* *rhythm-domain* *pitch-domain* *rhythm-domain* *pitch-domain* *rhythm-domain* *pitch-domain*))

;; Relaxed stress: keep 4v24 + all-different per voice + cross-voice no-unison;
;; remove melodic leap constraints to stay under default loop cap.
(defparameter *rules*
  (Rules->Cluster
    (R-pitches-one-voice #'all-diff-list? *voices* :all-pitches)
    (R-pitch-pitch (lambda (sim) (all-diff-list? (remove nil sim))) '(0 1 2 3) nil :all :exclude-gracenotes :pitch)))

(let ((res (time (ClusterEngine 24 nil nil *rules* '((4 4)) *domains*))))
  (format t "~%RESULT-OK: ~A~%" (if res t nil)))

(quit)
