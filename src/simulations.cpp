#include <vector>
#include <random>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "simulations.h"

#include <Rcpp.h>

using namespace Rcpp;

Rcpp::List simulation::do_simulation_mechanistic() {

    unsigned seed = static_cast<unsigned> (std::chrono::system_clock::now().time_since_epoch().count());
    rng.seed(seed);
    
    // prepare landscape and pop
    food.initResources();
    food.countAvailable();
    Rcpp::Rcout << "landscape with " << food.nClusters << " clusters\n";

    pop.setTrait(mSize);
    Rcpp::Rcout << "pop with " << pop.nAgents << " agents for " << genmax << " gens " << tmax << " timesteps\n";

    // prepare scenario
    // return scenario as string
    std::string scenario_str;
    switch (scenario)
    {
        case 0:
            Rcpp::Rcout << "this is scenario " << scenario << "random movement\n";
            break;
        case 1:
            Rcpp::Rcout << "this is scenario " << scenario << "optimal movement\n";
            break;
        case 2:
            Rcpp::Rcout << "this is scenario " << scenario << "evolved movement\n";
            break;
        
        default:
            Rcpp::Rcout << "unknown scenario\n";
            break;
    }

    // agent random position in first gen
    pop.initPos(food);

    Rcpp::Rcout << "initialised population positions\n";
    Rcpp::List edgeLists;

    Rcpp::Rcout << "created edge list object\n";

    // agent data logging increment
    int increment_log = std::max((static_cast<int>(static_cast<float>(genmax) * 0.001f)), 2);

    Rcpp::Rcout << "logging data after gens: " << increment_log << "\n";

    // go over gens
    for(int gen = 0; gen < genmax; gen++) {

        food.countAvailable();
        // Rcpp::Rcout << "food available = " << food.nAvailable << "\n";

        // reset counter and positions
        pop.counter = std::vector<int> (pop.nAgents, 0);
        
        // Rcpp::Rcout << "entering ecological timescale\n";

        // timesteps start here
        for (size_t t = 0; t < static_cast<size_t>(tmax); t++)
        {
            // resources regrow
            food.regenerate();
            pop.updateRtree();
            // movement section
            pop.move_mechanistic(food, nThreads);

            // log movement
            if(gen == std::max(gen_init - 1, 2)) {
                mdPre.updateMoveData(pop, t);
            }
            if(gen == (genmax - 1)) {
                mdPost.updateMoveData(pop, t);
            }

            // foraging -- split into parallelised picking
            // and non-parallel exploitation
            pop.pickForageItem(food, nThreads);
            pop.doForage(food);

            // count associations
            pop.countAssoc(nThreads);
            // timestep ends here
        }
        
        pop.energy = pop.intake;

        // update gendata
        if ((gen == (genmax - 1)) | (gen % increment_log == 0)) {

            // Rcpp::Rcout << "logging data at gen: " << gen << "\n";
            gen_data.updateGenData(pop, gen);
        }

        if((gen == 0) | ((gen % (genmax / 10)) == 0) | (gen == genmax - 1)) {
            edgeLists.push_back(pop.pbsn.getNtwkDf());
            Rcpp::Rcout << "gen: " << gen << " --- logged edgelist\n";
        }

        // reproduce
        pop.Reproduce(food, dispersal, mProb, mSize);

        // generation ends here
    }
    // all gens end here

    Rcpp::Rcout << "data prepared\n";

    return Rcpp::List::create(
        Named("gen_data") = gen_data.getGenData(),
        Named("edgeLists") = edgeLists,
        Named("move_pre") = mdPre.getMoveData(),
        Named("move_post") = mdPost.getMoveData()
    );
}

Rcpp::List simulation::do_simulation_random() {
    unsigned seed = static_cast<unsigned> (std::chrono::system_clock::now().time_since_epoch().count());
    rng.seed(seed);
    
    // prepare landscape and pop
    food.initResources();
    food.countAvailable();
    Rcpp::Rcout << "landscape with " << food.nClusters << " clusters\n";

    pop.setTrait(mSize);
    Rcpp::Rcout << "pop with " << pop.nAgents << " agents for " << genmax << " gens " << tmax << " timesteps\n";

    // prepare scenario
    // return scenario as string
    std::string scenario_str;
    switch (scenario)
    {
        case 0:
            Rcpp::Rcout << "this is scenario " << scenario << "random movement\n";
            break;
        case 1:
            Rcpp::Rcout << "this is scenario " << scenario << "optimal movement\n";
            break;
        case 2:
            Rcpp::Rcout << "this is scenario " << scenario << "evolved movement\n";
            break;
        
        default:
            Rcpp::Rcout << "unknown scenario\n";
            break;
    }

    // agent random position in first gen
    pop.initPos(food);

    Rcpp::Rcout << "initialised population positions\n";
    Rcpp::DataFrame edgeList;

    Rcpp::Rcout << "created single edge list object\n";

    // all ecological dynamics
    food.countAvailable();
    // reset counter and positions
    pop.counter = std::vector<int> (pop.nAgents, 0);

    // timesteps start here
    for (size_t t = 0; t < static_cast<size_t>(tmax); t++)
    {
        // resources regrow
        food.regenerate();
        pop.updateRtree();
        // movement section
        pop.move_random();

        mdPre.updateMoveData(pop, t);

        // foraging -- split into parallelised picking
        // and non-parallel exploitation
        pop.pickForageItem(food, nThreads);
        pop.doForage(food);

        // count associations
        pop.countAssoc(nThreads);
        // timestep ends here
    }
    pop.energy = pop.intake;
    
    // log population traits and outcomes
    gen_data.updateGenData(pop, 1);
    edgeList = pop.pbsn.getNtwkDf();

    Rcpp::Rcout << "gen: " << gen << " --- logged edgelist\n";
    Rcpp::Rcout << "data prepared\n";

    return Rcpp::List::create(
        Named("gen_data") = gen_data.getGenData(),
        Named("edgeLists") = edgeLists,
        Named("move_data") = mdPre.getMoveData()
    );
}

//' Runs an exploitation competition simulation returning structured output.
//'
//' @description Run the simulation using parameters passed as
//' arguments to the corresponding R function.
//'
//' @param scenario The scenario: 0 for random movement, 1 for optimal movement,
//' 2 for evolved mechanistic movement.
//' @param popsize The population size.
//' @param nItems How many food items on the landscape.
//' @param landsize The size of the landscape as a numeric (double).
//' @param nClusters Number of clusters around which food is generated.
//' @param clusterSpread How dispersed food is around the cluster centre.
//' @param tmax The number of timesteps per generation.
//' @param genmax The maximum number of generations per simulation.
//' @param range_perception The movement range for agents.
//' @param handling_time The handling time.
//' @param regen_time The item regeneration time.
//' @param nThreads How many threads to parallelise over. Set to 1 to run on
//' the HPC Peregrine cluster.
//' @param dispersal A float value; the standard deviation of a normal
//' distribution centred on zero, which determines how far away from its parent
//' each individual is initialised. The standard value is 5 percent of the
//' landscape size (\code{landsize}), and represents local dispersal.
//' Setting this to 10 percent is already almost equivalent to global dispersal.
//' @param mProb The probability of mutation. The suggested value is 0.01.
//' While high, this may be more appropriate for a small population; change this
//' value and \code{popsize} to test the simulation's sensitivity to these values.
//' @param mSize Controls the mutational step size, and represents the scale
//' parameter of a Cauchy distribution. 
//' @return An S4 class, `pathomove_output`, with simulation outcomes.
// [[Rcpp::export]]
S4 model_case_2(const int scenario,
                        const int popsize,
                        const int nItems, const float landsize,
                        const int nClusters,
                        const float clusterSpread,
                        const int tmax,
                        const int genmax,
                        const float range_perception,
                        const int handling_time,
                        const int regen_time,
                        const int nThreads,
                        const float dispersal,
                        const float mProb,
                        const float mSize) {

    // check that intial infections is less than popsize
    if(initialInfections > popsize) {
        Rcpp::stop("More infections than agents!");
    }
    // make simulation class with input parameters                            
    simulation this_sim(popsize, scenario, nItems, landsize,
                        nClusters, clusterSpread, tmax, genmax, 
                        range_perception,
                        handling_time, regen_time,
                        nThreads, dispersal,
                        mProb, mSize);

    // return scenario as string
    std::string scenario_str;
    switch (scenario)
    {
        case 0:
            scenario_str = std::string("random movement");
            // do the simulation using the simulation class function                        
            Rcpp::List simOutput = this_sim.do_simulation_random();
            break;
        case 1:
            scenario_str = std::string("optimal movement");
            // do the simulation using the simulation class function                        
            Rcpp::List simOutput = this_sim.do_simulation();
            break;
        case 2:
            scenario_str = std::string("evolved movement");
            // do the simulation using the simulation class function                        
            Rcpp::List simOutput = this_sim.do_simulation_mechanistic();
            break;
        
        default:
            scenario_str = std::string("unknown scenario");
            break;
    }

    // get generation data from output
    Rcpp::List gen_data = simOutput["gen_data"];
    // make list of dataframes of population traits
    Rcpp::List pop_data = gen_data["pop_data"];

    // parameter list
    Rcpp::List param_list = Rcpp::List::create(
            Named("scenario") = scenario_str,
            Named("generations") = genmax,
            Named("pop_size") = popsize,
            Named("pop_density") = static_cast<float>(popsize) / landsize,
            Named("item_density") = static_cast<float>(nItems) / landsize,
            Named("dispersal") = dispersal
        );

    // create S4 class pathomove output and fill slots
    S4 x("sim_output");
    x.slot("parameters") = Rcpp::wrap(param_list);
    x.slot("generations") = Rcpp::wrap(gen_data["gens"]);
    x.slot("trait_data") = Rcpp::wrap(pop_data);

    return(x);
}