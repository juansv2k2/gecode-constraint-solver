/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * Simple Musical Constraint Solver
 * Based on Gecode constraint programming toolkit
 * and inspired by JBS-Constraints
 * 
 * This example implements a basic melodic constraint solver
 * that generates musical sequences with no repeated notes.
 */

#include <gecode/driver.hh>
#include <gecode/int.hh>
#include <gecode/minimodel.hh>

using namespace Gecode;

/**
 * \brief Simple Musical Constraint Solver
 * 
 * Generates musical sequences of given length where:
 * - Notes are represented as MIDI note numbers (60-84 = C4-C6)
 * - No note repetitions are allowed (like JBS no-repetition-rule)
 * - Intervals between consecutive notes are constrained
 */
class MusicalSolver : public Script {
protected:
  /// Length of the musical sequence
  static const int sequence_length = 8;
  /// Lowest MIDI note (C4 = 60)
  static const int min_note = 60;
  /// Highest MIDI note (C6 = 84)  
  static const int max_note = 84;
  
  /// Array of musical notes (MIDI values)
  IntVarArray notes;
  /// Array of intervals between consecutive notes
  IntVarArray intervals;

public:
  /// Constraint variants
  enum {
    MODEL_BASIC,        ///< Basic no-repetition constraint
    MODEL_INTERVALS,    ///< Add interval constraints
    MODEL_MELODIC      ///< Add melodic movement constraints
  };

  /// Constructor
  MusicalSolver(const Options& opt) 
    : Script(opt), 
      notes(*this, sequence_length, min_note, max_note),
      intervals(*this, sequence_length-1, -24, 24) {
    
    // Enable tracing if requested
    if (opt.trace()) {
      trace(*this, notes, opt.trace());
      trace(*this, intervals, opt.trace());
    }

    // Constraint 1: No repeated notes (inspired by JBS no-repetition-rule)
    distinct(*this, notes, opt.ipl());

    // Calculate intervals between consecutive notes
    for (int i = 0; i < sequence_length-1; i++) {
      rel(*this, intervals[i] == notes[i+1] - notes[i], opt.ipl());
    }

    switch (opt.model()) {
    case MODEL_BASIC:
      // Only the no-repetition constraint
      break;
      
    case MODEL_INTERVALS:
      // Add interval size constraints (inspired by JBS interval rules)
      for (int i = 0; i < sequence_length-1; i++) {
        // No intervals larger than an octave (12 semitones)
        rel(*this, intervals[i], IRT_GQ, -12, opt.ipl());
        rel(*this, intervals[i], IRT_LQ, 12, opt.ipl());
        // No unisons (0 interval) - already handled by distinct constraint
      }
      break;
      
    case MODEL_MELODIC:
      // Add melodic movement constraints
      for (int i = 0; i < sequence_length-1; i++) {
        // Limit intervals to reasonable melodic steps
        rel(*this, intervals[i], IRT_GQ, -7, opt.ipl());  // No more than perfect 5th down
        rel(*this, intervals[i], IRT_LQ, 7, opt.ipl());   // No more than perfect 5th up
        
        // Avoid too many large leaps in a row
        if (i < sequence_length-2) {
          IntVar abs_int1(*this, 0, 12);
          IntVar abs_int2(*this, 0, 12);
          abs(*this, intervals[i], abs_int1, opt.ipl());
          abs(*this, intervals[i+1], abs_int2, opt.ipl());
          
          // If one interval is large (>4), next should be smaller
          rel(*this, (abs_int1 > 4) >> (abs_int2 <= 2), opt.ipl());
        }
      }
      break;
      
    default: 
      GECODE_NEVER;
    }

    // Branching strategy: choose variables with smallest domain first
    branch(*this, notes, INT_VAR_SIZE_MIN(), INT_VAL_MIN(), nullptr,
           [](const Space&, const Brancher&, unsigned int a,
              IntVar, int i, const int& n,
              std::ostream& o) {
             o << "note[" << i << "]"
               << ((a == 0) ? " = " : " != ")
               << n << " (MIDI)";
           });
  }

  /// Print solution in a musical format
  virtual void print(std::ostream& os) const {
    os << "Musical Sequence (MIDI notes): ";
    for (int i = 0; i < sequence_length; i++) {
      if (i > 0) os << " -> ";
      os << notes[i];
    }
    os << std::endl;
    
    os << "Intervals: ";
    for (int i = 0; i < sequence_length-1; i++) {
      if (i > 0) os << ", ";
      os << intervals[i].val();
    }
    os << std::endl;
    
    // Convert to note names for readability
    const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", 
                               "F#", "G", "G#", "A", "A#", "B"};
    os << "Note names: ";
    for (int i = 0; i < sequence_length; i++) {
      if (i > 0) os << " -> ";
      int midi = notes[i].val();
      int octave = (midi / 12) - 1;
      int pitch_class = midi % 12;
      os << note_names[pitch_class] << octave;
    }
    os << std::endl << std::endl;
  }

  /// Constructor for cloning
  MusicalSolver(MusicalSolver& s) : Script(s) {
    notes.update(*this, s.notes);
    intervals.update(*this, s.intervals);
  }
  
  /// Copy during cloning
  virtual Space* copy(void) {
    return new MusicalSolver(*this);
  }
};

/** \brief Main function */
int main(int argc, char* argv[]) {
  Options opt("Musical Constraint Solver");
  
  // Set up model variants
  opt.model(MusicalSolver::MODEL_BASIC);
  opt.model(MusicalSolver::MODEL_BASIC, "basic", "no repetitions only");
  opt.model(MusicalSolver::MODEL_INTERVALS, "intervals", "add interval constraints");
  opt.model(MusicalSolver::MODEL_MELODIC, "melodic", "add melodic movement rules");
  
  // Set solution limit
  opt.solutions(10);  // Find up to 10 solutions
  opt.iterations(1000);
  
  opt.parse(argc, argv);
  
  Script::run<MusicalSolver, DFS, Options>(opt);
  return 0;
}

// Build with: g++ -std=c++11 musical-solver.cpp -lgecode -lgecodedriver -lgecodeminimodel -lgecodeint -lgecodesearch -lgecodekernel -lgecodesupport -o musical-solver