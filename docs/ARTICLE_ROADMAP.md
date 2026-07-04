# Article Roadmap — Neurosymbolic Polyphonic Constraint Solving

**Target publication:** Computer Music Journal (MIT Press) or Journal of New Music Research (JNMR, Taylor & Francis).  
Secondary targets: ICMC proceedings, SMC proceedings, NIME.

**Working title:**  
*"Neurosymbolic Score Generation: Integrating Neural Value Ordering into a Polyphonic Constraint Solver"*

---

## 1. Core Argument / Novelty Statement

Most music generation research treats the problem as either:
- **Generative modelling** (language models, diffusion, VAEs) — statistically fluent but structurally unconstrained, or  
- **Formal constraint satisfaction** (CP, rule systems) — structurally exact but musically rigid.

This work proposes a third position: **constraint solving as the correctness layer, neural scoring as the style layer**. The neural model never controls which solutions are valid — it only biases which valid solutions the solver finds first. This guarantees that formal musical rules (twelve-tone rows, palindromes, metric hierarchy, counterpoint voice-leading) are respected unconditionally, while allowing learned stylistic preferences to guide the search through the solution space.

The key contribution is the **value-ordering hook** as a clean integration seam between a formal constraint solver (Gecode) and an MLP trained on a large musical corpus. No architecture changes to the solver are required; the neural component is a pluggable module. This design principle generalises to any CP solver with a programmable value selector.

---

## 2. Article Outline

### Section 1 — Introduction (~800 words)
- The gap between generative neural models and formal composition systems
- Motivation: composer-facing tools that enforce formal structure while learning from corpora
- Cluster Engine (Sandred) as the intellectual precursor — polyphonic dynamic constraints
- Contribution summary: neurosymbolic architecture, implementation, evaluation on counterpoint and chord-conditioned melody
- Roadmap of the paper

### Section 2 — Related Work (~1 200 words)
- **Constraint-based composition systems:** PWGL / OpenMusic / Cluster Engine, Situation (Laurson/Duthen), Omi (Laurson), AC Toolbox, OpenMusic OMClouds
- **Neural music generation:** RNN/LSTM era (Magenta, DeepBach), Transformer era (Music Transformer, MusicLM), diffusion models for audio
- **Hybrid neural-symbolic approaches:** Anticipation-RNN (Hadjeres & Nielsen 2017), DeepSaDe (AAAI 2024), physics-informed neural networks as parallel in other domains
- **Constraint-guided decoding in NLP:** constrained beam search, FUDGE, COLD decoding — motivation for why the CP approach is stronger than decoding-time heuristics for discrete musical domains
- **Gap:** formal polyphonic constraint solving + learned value ordering has not been studied. Most existing work either uses constraints only as hard filters on generative model outputs (post-hoc) or embeds soft preferences into ad hoc rule weights.

### Section 3 — System Architecture (~1 000 words)
- Overview diagram: JSON config → solver → MusicXML + JSON output
- Three layers: **domain layer** (voice pitch/rhythm/metric domains), **constraint layer** (hard built-in and dynamic wildcard rules), **heuristic layer** (value-ordering, neural + symbolic buckets)
- Multi-voice engine: each voice = rhythm engine + pitch engine; metric engine coordinates time signatures
- Max/MSP integration: same core, real-time capable via gecode.solver Max external
- JSON-driven configuration philosophy: reproducible, version-controlled, human-readable scores

### Section 4 — Constraint Formulation (~1 200 words)
- Variable representation: pitch variables (MIDI int domain), rhythm variables (rational duration domain), metric variables (time-signature domain)
- Built-in constraint families: single-voice (all_different, uniformity), cross-voice pitch (interval constraints, no parallel fifths/octaves, contrary motion), cross-voice rhythm (isorhythm, augmentation, diminution, rest complement), metric (metric hierarchy, no syncopation, hierarchical voices / cantus firmus)
- Wildcard constraints: indexed expression evaluation over voice/position pairs; `for_all_positions`, `for_all_voices`, `sliding_window`
- Dynamic rule compiler: expression string → compiled predicate (basic_constraint) or scoring function (heuristic_energy)
- Soft constraint representation: `"heuristic": true` on built-in rules converts them to preference-only; they rank but do not prune

### Section 5 — Neural Value Ordering (~1 500 words)
- The value-ordering hook in Gecode: `IntValBranch` and custom value-selection functors
- Priority bucket architecture: primary (neural MLP), secondary (symbolic heuristic_energy rules), tertiary (soft built-in rules), fallback (min/random)
- MLP architecture: 3-layer feedforward, input = [pitch context (8), interval context (8), mod-12 pitch class (12), chord one-hot (24), voice id (4), position norm (4)] = 60 dimensions; hidden = 256; output = 128-class softmax over MIDI candidates
- Training: 67k labeled Bach events (music21 corpus), chord labelling via `isDominantSeventh()` / `isMajorTriad()` / `isMinorTriad()` (workaround for music21 `Chord.quality` dom7 bug); validation accuracy ~50%
- Harmonic conditioning: `harmonic_domain` JSON array → forward-filled `harmonic_state` vector → per-position chord class; passed to scorer at call time
- Gumbel-max sampling: `score_i = logit_i / T + gumbel(seed, voice, pos, candidate)` — stochastic diversity with temperature control
- Correctness invariant: neural score affects proposal order only; hard constraints checked independently; backtracking proceeds normally if top-scored candidates violate rules
- Latency analysis: < 1ms per candidate on M-series Apple Silicon; profiling breakdown (feature extraction / forward pass / sorting)
- Key engineering bug: `forward_clf` loop bound was `context_size` (8) instead of `input_size` (60) — rhythm, voice, and chord dimensions silently ignored until fixed
- Key engineering bug: `musical_constraint_solver.cpp` overwrote neural scorer on every `solve()` — fixed by saving prior scorer via `get_pitch_heuristic_value_ordering()` and chaining

### Section 6 — Evaluation (~1 500 words)

#### Experiment 1: Chord-Tone Accuracy (controlled isolation test)
- Config: 1 voice, 16 notes, chromatic domain C4–B5 (24 pitches), minimal hard constraints (no adjacent repeat, no pitch[i] == pitch[i+2])
- Chord progression: C major → F major → G dom7 → C major
- Condition A: `value_order: "min"` (no neural)
- Condition B: `value_order: "neural"`, T=1.0
- Condition C: `value_order: "neural"`, T=0.3
- Metric: % of notes that are chord tones
- Result: C achieves 100% chord-tone rate; B approaches same; A shows no preference

#### Experiment 2: Counterpoint Quality
- Config: 2 voices (soprano + bass), C-major pitch domains, fixed quarter-note rhythm, hard rules = soprano above bass + max two-octave spread + max P5 soprano leap
- Soft rules = prefer consonant harmony, no parallel fifths/octaves, prefer contrary motion (all `heuristic: true`)
- Condition A: `value_order: "heuristic"` (symbolic only)
- Condition B: `value_order: "neural"` + same soft rules as tiebreakers
- Metrics: % consonant intervals at each position, % parallel fifths/octaves, % contrary motion steps, subjective listening evaluation
- Expected result: B shows measurably more chord-tone bias; A shows more stepwise variety; both satisfy all hard rules equally

#### Experiment 3: Structural Constraint Compatibility
- Show that twelve-tone row + palindrome + neural scorer all coexist correctly: no row notes repeated, palindrome satisfied, neural scorer active
- Verify with `heuristic_trace` output that neural scores vary per position but no violation of structural rules occurs

#### Experiment 4: Solve Time Comparison
- Benchmark neural vs non-neural across configs of increasing complexity (8, 16, 32, 64 positions; 1–4 voices)
- Metric: mean solve time (ms), search nodes explored
- Expected result: neural reduces nodes by guiding toward valid solutions faster; wall time increases slightly due to scoring overhead but remains < 100ms for typical compositional tasks

### Section 7 — Discussion (~800 words)
- The neurosymbolic seam: why the value-ordering hook is the right integration point (not domain pruning, not learned backtracking, not post-hoc filtering)
- Comparison to Anticipation-RNN: that approach injects a future constraint as a conditioning signal for generation; this approach uses a trained scorer inside a complete CP search with any combination of hard rules
- Composer workflow implications: formalize structure in JSON, delegate style to a pre-trained model, get diverse solutions satisfying all formal requirements
- Limitations: MLP trained on Bach/jazz; no phrase-level context; no rhythm neural scorer yet; `heuristic_top_k` interaction with domain ordering
- The soft/hard contract: implications for auditability and formal verification of generated scores

### Section 8 — Future Work (~500 words)
- Phase 4: separate rhythm MLP scorer (parallel architecture)
- Phase 5: joint pitch+rhythm scorer capturing rhythmic-melodic correlations
- Phase 6: small GRU/LSTM for phrase-level context
- Phase 7: encoder-only Transformer (sequence length 8–16)
- Phase 8: style-conditioned scorer via runtime style embedding
- Online adaptation: update scorer weights during a composing session based on user feedback
- Multi-objective search: Pareto-optimal solutions balancing multiple neural scorers
- Rhythm scorer training: Weimar Jazz Database rhythm feature pipeline

### Section 9 — Conclusion (~300 words)
- Neurosymbolic position restated: formal rules for structure, neural models for style
- Practical result: 100% chord-tone rate achievable with standard counterpoint constraints intact
- Open-source release: JSON config format, C++17 solver, Max external, training scripts

---

## 3. Figures to Produce

| # | Description | Source |
|---|---|---|
| 1 | System architecture diagram (3 layers: domain / constraint / heuristic) | Diagram to create |
| 2 | Value-ordering bucket priority stack (neural → symbolic → soft-built-in → fallback) | Diagram to create |
| 3 | MLP input feature vector (60 dims, colour-coded by group) | Diagram to create |
| 4 | Chord-tone accuracy bar chart across conditions (Exp 1) | From experiment results |
| 5 | MusicXML score excerpt: chord-conditioned 1-voice output (C/F/G7/C) | `output/chromatic_chord_test.xml` |
| 6 | MusicXML score excerpt: 2-voice neural counterpoint | `output/neural_counterpoint_soprano_bass.xml` |
| 7 | Solve-time vs. positions plot (neural vs. baseline, 4 voice configurations) | From benchmark |
| 8 | Heuristic trace visualization: per-candidate neural scores at one variable | From `heuristic_trace: true` output |

---

## 4. Open Questions / Things to Nail Down Before Submission

- [ ] **Musical quality metric**: chord-tone accuracy is quantifiable; find or construct a counterpoint quality metric (e.g. % consonant intervals, % parallel fifths, % contrary motion) for Experiment 2
- [ ] **Comparison baseline**: identify a comparable hybrid system to compare against (Anticipation-RNN is the closest; DeepBach for Bach style)
- [ ] **Validation accuracy of 50%**: clarify whether this is top-1 or top-5 accuracy; 50% top-1 on a 128-class problem is competitive but should be framed correctly
- [ ] **Training dataset**: Bach via music21 (67k rows) + Weimar Jazz (future). Need exact corpus citation and size
- [ ] **Rhythm scorer**: decide whether to include rhythm neural scorer in the paper (future work) or wait for it to be complete
- [ ] **User study**: worth including even a small informal listening study for the counterpoint experiment to support subjective quality claims
- [ ] **Code release**: prepare a clean public-facing branch with example configs and pre-trained weights before submission

---

## 5. Candidate References (to expand)

- Sandred, Ö. — Cluster Engine and CARMA system (multiple papers)
- Hadjeres, G. & Nielsen, F. — *Anticipation-RNN* (ISMIR 2017)
- Huang, C-Z. A. et al. — *Music Transformer* (ICLR 2019)
- Laurson, M. & Duthen, J. — *PatchWork* (ICMC 1989)
- Assayag, G. et al. — *Computer-assisted composition at IRCAM* (CMJ 1999)
- Pachet, F. & Roy, P. — *Musical harmonization with constraints* (Knowledge-Based Systems 2001)
- Fages, F. et al. — *Principles and Practice of Constraint Programming* (CP Springer series)
- Verfaille, V. et al. — *Constraint programming for music* (survey, JNMR 2006)
- Malik, I. & Ek, C. H. — *Neural Temporal Point Processes* (NeurIPS 2021, for rhythm)
- Agres, K. et al. — *Music generation by deep learning: challenges and directions* (Neural Computing 2017)

---

## 6. Target Word Count

| Section | Target |
|---|---|
| Abstract | 250 |
| 1. Introduction | 800 |
| 2. Related Work | 1 200 |
| 3. System Architecture | 1 000 |
| 4. Constraint Formulation | 1 200 |
| 5. Neural Value Ordering | 1 500 |
| 6. Evaluation | 1 500 |
| 7. Discussion | 800 |
| 8. Future Work | 500 |
| 9. Conclusion | 300 |
| **Total** | **~9 050** |

CMJ typical article length: 6 000–10 000 words. JNMR: 5 000–8 000. ICMC: 4–6 pages (≈ 2 500–4 000 words for proceedings format).  
**Recommendation: target CMJ/JNMR full article. Prepare a 4-page ICMC version as a shorter parallel submission.**

---

## 7. Writing Status

| Section | Status | Notes |
|---|---|---|
| 1. Introduction | not started | |
| 2. Related Work | not started | survey in `docs/NEURAL_RESEARCH_SURVEY.md` — mine this |
| 3. Architecture | not started | |
| 4. Constraints | not started | USAGE.md Section 5 is a detailed source |
| 5. Neural | not started | `docs/NEURAL_HEURISTIC_INTEGRATION.md` is the technical source |
| 6. Evaluation | not started | experiments not yet formally run |
| 7. Discussion | not started | |
| 8. Future | not started | roadmap in `docs/NEURAL_HEURISTIC_INTEGRATION.md` §Research Progression |
| 9. Conclusion | not started | |
| Figures | not started | |
