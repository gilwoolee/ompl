/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2010, Rice University
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Rice University nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include "ompl/geometric/Benchmark.h"
#include "ompl/geometric/PathGeometric.h"
#include "ompl/util/Time.h"
#include "ompl/util/Memory.h"
#include <fstream>
#include <sstream>

void ompl::geometric::Benchmark::saveResultsToFile(const char *filename) const
{
    std::ofstream fout(filename);
    saveResultsToStream(fout);
}

void ompl::geometric::Benchmark::saveResultsToStream(std::ostream &out) const
{
    out << exp_.size() << " planners" << std::endl;
    for (unsigned int i = 0 ; i < exp_.size() ; ++i)
    {
	out << exp_[i].name;
	
	// construct the list of all possible properties for all runs
	std::map<std::string, bool> propSeen;
	for (unsigned int j = 0 ; j < exp_[i].runs.size() ; ++j)
	    for (std::map<std::string, std::string>::const_iterator mit = exp_[i].runs[j].begin() ;  mit != exp_[i].runs[j].end() ; ++mit)
		propSeen[mit->first] = true;
	std::vector<std::string> properties;
	for (std::map<std::string, bool>::iterator it = propSeen.begin() ; it != propSeen.end() ; ++it)
	    properties.push_back(it->first);
	std::sort(properties.begin(), properties.end());
	
	// print the property names
	out << properties.size() << " properties for each run" << std::endl;
	for (unsigned int j = 0 ; j < properties.size() ; ++j)
	    out << properties[j] << std::endl;

	// print the data for each run
	out << exp_[i].runs.size() << " runs" << std::endl;
	for (unsigned int j = 0 ; j < exp_[i].runs.size() ; ++j)
	{
	    for (unsigned int k = 0 ; k < properties.size() ; ++k)
	    {
		std::map<std::string, std::string>::const_iterator it = exp_[i].runs[j].find(properties[k]);
		if (it != exp_[i].runs[j].end())
		    out << it->second;
		out << "; ";
	    }
	    out << std::endl;
	}

	// get names of averaged properties
	properties.clear();
	for (std::map<std::string, std::string>::const_iterator mit = exp_[i].avg.begin() ;  mit != exp_[i].avg.end() ; ++mit)
	    properties.push_back(mit->first);
	std::sort(properties.begin(), properties.end());
	
	// print names & values of averaged properties
	out << properties.size() << " averaged properties" << std::endl;
	for (unsigned int k = 0 ; k < properties.size() ; ++k)
	{
	    std::map<std::string, std::string>::const_iterator it = exp_[i].avg.find(properties[k]);
	    out << it->first << " = " << it->second << std::endl;
	}
	out << '.' << std::endl;
    }
}

namespace ompl
{    
    template <typename T>
    static std::string toString(const T &value)
    {
	std::stringstream ss;
	ss << value;
	return ss.str();
    }	
}

void ompl::geometric::Benchmark::benchmark(double maxTime, double maxMem, unsigned int runCount)
{
    // sanity checks
    if (!si_)
    {
	msg_.error("No space information defined");
	return;
    }

    if (!getGoal())
    {
	msg_.error("No goal defined");
	return;
    }
    
    if (!si_->isSetup())
	si_->setup();

    // the set of properties to be averaged, for each planner
    std::vector<std::string> avgProperties;
    avgProperties.push_back("solved");
    avgProperties.push_back("time");
    avgProperties.push_back("memory");
    
    // clear previous experimental data
    exp_.clear();
    exp_.resize(planners_.size());    
    
    for (unsigned int i = 0 ; i < planners_.size() ; ++i)
    {
	// configure the planner
	planners_[i]->setProblemDefinition(pdef_);
	if (!planners_[i]->isSetup())
	    planners_[i]->setup();
	exp_[i].name = planners_[i]->getName();
	
	// run the planner 
	for (unsigned int j = 0 ; j < runCount ; ++j)
	{
	    // make sure there are no pre-allocated states and all planning data structures are cleared
	    si_->getStateAllocator().clear();
	    planners_[i]->clear();
	    getGoal()->clearSolutionPath();
	    
	    MemUsage_t memStart = getProcessMemoryUsage();
	    time::point timeStart = time::now();

	    // \todo make sure the planned terminates when memory limit is passed
	    bool solved = planners_[i]->solve(maxTime);

	    double timeUsed = time::seconds(time::now() - timeStart);
	    MemUsage_t memUsed = getProcessMemoryUsage() - memStart;
	    
	    RunProperties run;
	    run["solved"] = toString(solved);
	    run["time"] = toString(timeUsed);
	    run["memory"] = toString((double)memUsed / (1024.0 * 1024.0));
	    run["preallocated states"] = toString(si_->getStateAllocator().size());
	    if (solved)
	    {
		run["approximate solution"] = toString(getGoal()->isApproximate());
		run["solution difference"] = toString(getGoal()->getDifference());
		run["solution length"] = toString(static_cast<PathGeometric*>(getGoal()->getSolutionPath().get())->length());
	    }
	    
	    base::PlannerData pd;
	    planners_[i]->getPlannerData(pd);
	    run["graph states"] = toString(pd.states.size());
	    unsigned long edges = 0;
	    for (unsigned int k = 0 ; k < pd.edges.size() ; ++k)
		edges += pd.edges[k].size();
	    run["graph motions"] = toString(edges);
	    
	    // \todo get planner specific properties

	    exp_[i].runs.push_back(run);
	}

	// compute averages
	for (unsigned int p = 0 ; p < avgProperties.size() ; ++p)
	{
	    double sum = 0.0;
	    for (unsigned int j = 0 ; j < exp_[i].runs.size() ; ++i)
	    {
		std::stringstream ss(exp_[i].runs[j][avgProperties[p]]);
		double value;
		ss >> value;
		sum += value;
	    }
	    exp_[i].avg[avgProperties[p]] = toString(sum / (double)exp_[i].runs.size());
	}
    }    
}