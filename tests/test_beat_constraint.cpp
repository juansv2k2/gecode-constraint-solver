/**
 * @brief Minimal Gecode test to verify beat alignment constraint.
 * Tests: if abs(x) == 4 (tuplet tick), then cumulative sum of previous
 * abs values must be divisible by 12 (beat_ticks for 4/4 with base 48).
 */
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>
#include <iostream>
#include <vector>

using namespace Gecode;

static const int N = 12;             // sequence length
static const int BEAT_TICKS = 12;
static const int TUPLET_TICK = 4;
static const int MAX_ONSET = 12 * N; // worst case: all 12-tick notes

class BeatTest : public Space {
public:
    IntVarArray rhythm; // domain: {-12,-6,-4,-3,3,4,6,12}
    IntVarArray onset;  // onset[i] = sum of abs(rhythm[0..i-1])
    IntVarArray abs_r;  // abs_r[i] = abs(rhythm[i])

    explicit BeatTest(bool first_fail = false)
        : rhythm(*this, N, -12, 12),
          onset(*this, N, 0, MAX_ONSET),
          abs_r(*this, N, 0, 12)
    {
        const std::vector<int> dom_vals = {-12, -6, -4, -3, 3, 4, 6, 12};
        
        // Set rhythm domain
        IntSet rdom(dom_vals.data(), dom_vals.size());
        for (int i = 0; i < N; ++i) {
            dom(*this, rhythm[i], rdom);
        }
        
        // onset[0] = 0
        rel(*this, onset[0], IRT_EQ, 0);
        
        // abs[i] = |rhythm[i]|
        for (int i = 0; i < N; ++i) {
            abs(*this, rhythm[i], abs_r[i], IPL_DOM);
        }
        
        // onset[i] = onset[i-1] + abs_r[i-1]
        for (int i = 1; i < N; ++i) {
            linear(*this, IntArgs({1,-1,-1}), 
                   IntVarArgs({onset[i], onset[i-1], abs_r[i-1]}),
                   IRT_EQ, 0, IPL_DOM);
        }
        
        // beat_set: {0, 12, 24, 36, ...}
        std::vector<int> beat_vals;
        for (int o = 0; o <= MAX_ONSET; o += BEAT_TICKS) beat_vals.push_back(o);
        IntSet beat_set(beat_vals.data(), beat_vals.size());
        
        // mid_set: multiples of TUPLET_TICK NOT on beat
        std::vector<int> mid_vals;
        for (int o = TUPLET_TICK; o <= MAX_ONSET; o += TUPLET_TICK)
            if (o % BEAT_TICKS != 0) mid_vals.push_back(o);
        IntSet mid_set(mid_vals.data(), mid_vals.size());

        // Constraint: for each position i, if abs_r[i] == TUPLET_TICK, 
        // then onset[i] must be in beat_set
        for (int i = 0; i < N; ++i) {
            BoolVar is_on_beat(*this, 0, 1);
            dom(*this, onset[i], beat_set, is_on_beat);
            
            BoolVar using_tuplet = expr(*this, abs_r[i] == TUPLET_TICK);
            rel(*this, using_tuplet, IRT_LQ, is_on_beat);

            // closure: onset in mid_set → using_tuplet
            BoolVar is_mid(*this, 0, 1);
            dom(*this, onset[i], mid_set, is_mid);
            rel(*this, is_mid, IRT_LQ, using_tuplet);
        }
        
        if (first_fail)
            branch(*this, rhythm, INT_VAR_SIZE_MIN(), INT_VAL_MIN());
        else
            branch(*this, rhythm, INT_VAR_NONE(), INT_VAL_MIN());
    }
    
    BeatTest(BeatTest& s) 
        : Space(s),
          rhythm(s.rhythm), onset(s.onset), abs_r(s.abs_r)
    {
        rhythm.update(*this, s.rhythm);
        onset.update(*this, s.onset);
        abs_r.update(*this, s.abs_r);
    }
    
    virtual Space* copy() { return new BeatTest(*this); }
    
    void print() const {
        std::cout << "Rhythm: ";
        int o = 0;
        for (int i = 0; i < N; ++i) {
            int v = rhythm[i].val();
            if (v > 0) std::cout << "+" << v;
            else std::cout << v;
            // Check violation
            if (std::abs(v) == TUPLET_TICK && o % BEAT_TICKS != 0) {
                std::cout << "(VIOLATION! onset=" << o << ")";
            }
            o += std::abs(v);
            std::cout << " ";
        }
        std::cout << "\n";
    }
};

int main() {
    for (bool first_fail : {false, true}) {
        int total = 0, violations = 0;
        Gecode::Search::Options opts;
        Gecode::Search::TimeStop stop(5000); // 5 seconds max
        opts.stop = &stop;
        DFS<BeatTest> engine(new BeatTest(first_fail), opts);
        while (BeatTest* sol = engine.next()) {
            total++;
            // Check for violations
            int o = 0;
            for (int i = 0; i < N; ++i) {
                int v = sol->rhythm[i].val();
                if (std::abs(v) == TUPLET_TICK && o % BEAT_TICKS != 0) {
                    violations++;
                    if (violations <= 3) {
                        std::cout << "VIOLATION (ff=" << first_fail << "): ";
                        sol->print();
                    }
                }
                o += std::abs(v);
            }
            delete sol;
            if (total >= 100000) break; // sample limit
        }
        std::cout << "branching=" << (first_fail ? "first_fail" : "input_order")
                  << "  N=" << N
                  << "  solutions=" << total
                  << "  violations=" << violations << "\n";
    }
    return 0;
}
