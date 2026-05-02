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

(defun all-diff-list? 
    (xs)
    (if 
        (null xs) t
        (and 
            (not 
                (member 
                    (car xs) 
                    (cdr xs) :test #'equal))
            (all-diff-list? 
                (cdr xs)))))

(defun retrograde-2v-12? 
    (a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11)
    (and 
        (= 
            (first a0) 
            (second a11))
        (= 
            (first a1) 
            (second a10))
        (= 
            (first a2) 
            (second a9))
        (= 
            (first a3) 
            (second a8))
        (= 
            (first a4) 
            (second a7))
        (= 
            (first a5) 
            (second a6))
        (= 
            (first a6) 
            (second a5))
        (= 
            (first a7) 
            (second a4))
        (= 
            (first a8) 
            (second a3))
        (= 
            (first a9) 
            (second a2))
        (= 
            (first a10) 
            (second a1))
        (= 
            (first a11) 
            (second a0))))

(defparameter *voices* '
    (0 1))
(defparameter *pitch-domain*
'
    (
        (60) 
        (61) 
        (62) 
        (63) 
        (64) 
        (65) 
        (66) 
        (67) 
        (68) 
        (69) 
        (70) 
        (71)))
(defparameter *rhythm-domain* '
    (
        (1/4)))
(defparameter *domains*
    (list *rhythm-domain* *pitch-domain* *rhythm-domain* *pitch-domain*))
(defparameter *timepoints*
'
    (0 1/4 2/4 3/4 4/4 5/4 6/4 7/4 8/4 9/4 10/4 11/4))

(defparameter *rules*
    (Rules->Cluster
        (R-pitches-one-voice #'all-diff-list? *voices* :all-pitches)
        (R-pitch-pitch #'retrograde-2v-12? '
            (0 1) *timepoints* :at-timepoints :exclude-gracenotes :pitch)))

(let 
    (
        (res 
            (time 
                (ClusterEngine 12 t nil *rules* '
                    (
                        (4 4)) *domains*))))
    (format t "~%RESULT-OK: ~A~%" 
        (if res t nil)))

(quit)
