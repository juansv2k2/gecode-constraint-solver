# Beethoven Sketch Extender — Design Decisions

**Status:** agreed direction, September 2026
**Audience:** project team
**Scope:** the generative module only — orchestration is a separate module

---

## 1. What this module does

Given a short Beethoven sketch (Kafka edition — usually a single melodic line, sometimes with a bass hint), produce a **coherent 16–32 bar continuation in 4 real parts**.

The output is a symbolic 4-part realization. Orchestration happens downstream in a separate Python module and is out of scope here.

```
MIDI / MIR state → [retrieval] → sketch
sketch + section spec → [neural generator] → prior + chord plan
prior + rules → [constraint solver] → 4-part realization
realization → [orchestrator] → score
```

---

## 2. Form: fantasia, not sonata

**Decision: the piece is organized as a fantasia — sectional, contrast-driven, with free return of material.**

Sonata form requires two things that are genuinely hard:

- **Exact thematic recall with transposition.** The recapitulation must restate the second theme in the home key. That is a dependency stretching across hundreds of bars plus a specific transformation.
- **Functional tonal architecture.** Establishing a secondary key and then resolving it is the entire point. Get it wrong and the form is not "different," it is meaningless.

Fantasia needs only sectional contrast, coherent local motion, and clear cadences between sections. Material may return, but it does not have to.

### This is not a compromise

Fantasia is arguably _more_ faithful to the project premise:

- Op. 27 is _quasi una fantasia_.
- Op. 77 is the free-improvisatory Beethoven.
- **Choral Fantasy Op. 80 is a fantasia for piano soloist and orchestra** — the exact performance configuration we are building.

There is also a systems reason. Our live path is retrieval-driven: performance state selects a sketch, which generates a new section. That is inherently sectional and improvisatory. Sonata form would _fight_ the retrieval mechanism, because it demands structural returns at predetermined moments that retrieval cannot guarantee.

**Optional coherence insurance:** fantasia with one recurring idea (ritornello). Cheap to enforce as a rule, and it gives the audience something to hold onto.

---

## 3. The core principle

> **Constraints own structure. The neural model owns surface.**

| Constraints decide                                     | The model decides               |
| ------------------------------------------------------ | ------------------------------- |
| Where cadences land                                    | Melodic detail                  |
| How phrases relate to each other                       | Voice-leading realization       |
| Motivic operations (inversion, sequence, augmentation) | Register and spacing            |
| The chord plan                                         | Texture and rhythmic figuration |

Neural models learn cadences statistically but place them _unreliably_. Constraints place them exactly where the form requires. Since formal control is the whole point of this project, structure goes in the solver.

This principle should be the tiebreaker whenever we are unsure where a feature belongs.

---

## 4. The neural model

**Masked multi-voice transformer. Roughly 8–12 layers, `d_model` 256–512, 5–20M parameters.**

Three properties matter:

**It sees all voices at once.** Voice leading, spacing and doubling are relationships _between_ parts. Our current model only sees one voice at a time, which makes 4-part writing impossible for it.

**It can be queried in any order.** The solver assigns variables in smallest-domain-first order, not left to right. A normal left-to-right model (GPT-style, LSTM) cannot condition on notes the solver has already fixed _later_ in the bar. A masked model can. This is the decisive technical reason for the choice.

**It is small on purpose.** With our realistic data volume, a large model would memorize Beethoven rather than learn his procedures — producing quotations, which is both an artistic and a rights problem.

### One model, five jobs

Because it is trained by masking, the same model performs different tasks depending on what we hide:

| Task                     | What we mask                          |
| ------------------------ | ------------------------------------- |
| Continue the sketch      | Future bars                           |
| Harmonize a melody       | Everything except the top line        |
| Fill a gap               | Middle bars                           |
| Re-texture, keep harmony | Inner voices only                     |
| Vary a phrase            | A region, with a different chord plan |

This matters because Kafka sketches are usually **single lines**. Going from one line to four parts is harmonization, not just continuation — and the same model handles both.

---

## 5. Training data

Because the task is _extension_, training examples are self-supervised: take any passage, hide part of it, learn to fill it in. Effectively unlimited pairs from a modest corpus.

| Source                            | ~Movements | Why                                  |
| --------------------------------- | ---------- | ------------------------------------ |
| Haydn quartets                    | 270        | Exactly 4 real parts, Classical      |
| Mozart quartets/quintets          | 100        | Same                                 |
| Beethoven quartets                | 65         | Target idiom                         |
| Piano sonatas reduced to 4 voices | 500        | Large, stylistically adjacent        |
| Bach chorales                     | 400        | Best voice-leading reference we have |

**String quartets are the key resource.** They are 4 real parts in the right style, and there are far more of them than symphonies.

---

## 6. New rule modules for the solver

Five additions, extending machinery the solver already has:

**`r-cadence`** — place a cadence of a given type at a given position.
Types: PAC, IAC, half, deceptive, plagal. Specifies bass motion, soprano target, inversion, and required metric strength.

**`r-phrase`** — declares phrase boundaries; implies a cadence at each one.

**`r-repetition`** — material in range A relates to range B by: `exact`, `transposed(n)`, `contour_preserving`, `rhythm_preserving`, or `diatonic_sequence`.
This one rule covers sentence structure, harmonic sequence, antecedent/consequent pairs, and "same tune, different harmony" (same pitch relation, different chord plan over the target).

**`r-motif`** — inversion, retrograde, augmentation, diminution, fragmentation.
_We already have most of this._ `palindrome_of_engine` is retrograde; the twelve-tone row handling is the same family. This is a generalization, not new research.

**`r-harmonic-rhythm`** — rate of chord change per bar, allowed to accelerate into cadences. One of the strongest style signals in Classical music and cheap to express.

### Hard vs. soft

The solver supports `"heuristic": true` to make a rule a preference instead of a requirement. **Use it.** If cadences _and_ phrase parallelism _and_ motivic relations _and_ voice leading are all hard constraints simultaneously, the set of legal solutions collapses and the neural model has no room to express anything.

**Default: cadences hard, phrase and motivic relations soft.**

---

## 7. Module boundaries

The generator is Python and will be retrained often. The solver is C++ and should stay stable. **Do not link them.**

The contract between modules is **data, not code**. The generator writes an artifact; the solver reads it as just another input file alongside the config JSON.

This keeps the C++ side free of Python and PyTorch, and lets the orchestration module sit behind an equally clean boundary.

**Version-stamp every artifact format** — weights, block JSON, and prior tensor. We already do this in the weights file (`model_type`, `uses_harmonic_conditioning`); extend the same discipline everywhere.

### Latency

Not a concern on this path. A forward pass is milliseconds, the orchestra needs lead time to read anyway, and we generate section _N+1_ while section _N_ is playing. Keep sections at 16–32 bars so solve times stay short.

---

## 8. Known risks

**Sketch → 4 parts is harmonization.** If the training masks do not include melody-only conditioning, the model will be weakest at exactly the case we need most.

**Blandness.** Iterative refinement with a small model tends to regress toward the average — correct but inert. Plan an entropy floor or temperature schedule from the start rather than bolting one on later.

**Phrase labels do not exist.** Cadence detection and phrase segmentation must be derived from the corpus before we can train or evaluate anything. This is real preprocessing work, not a given.

**Constraint pressure may squeeze out the model.** If the rules are too tight, every high-probability candidate gets pruned and the model stops mattering. We have already seen a small version of this — a hard consonance rule was masking the neural signal entirely.

---

## 9. First step

**Build `r-cadence` and `r-repetition` against the current model, at 4 voices, before training anything new.**

Three reasons:

1. It validates the rule design and the hard/soft balance.
2. It is work we need regardless of which model we end up with.
3. It answers the biggest open question quickly — _does constraint pressure squeeze out the neural signal at 4 voices?_

If that holds up, the model work is comparatively low-risk.
