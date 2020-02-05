/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <test/yulPhaser/Common.h>

#include <tools/yulPhaser/Chromosome.h>
#include <tools/yulPhaser/Population.h>
#include <tools/yulPhaser/Program.h>

#include <libyul/optimiser/BlockFlattener.h>
#include <libyul/optimiser/SSAReverser.h>
#include <libyul/optimiser/StructuralSimplifier.h>
#include <libyul/optimiser/UnusedPruner.h>

#include <liblangutil/CharStream.h>

#include <boost/test/unit_test.hpp>

#include <optional>
#include <string>
#include <sstream>

using namespace std;
using namespace solidity::langutil;
using namespace solidity::yul;
using namespace boost::unit_test::framework;

namespace solidity::phaser::test
{

class PopulationFixture
{
protected:
	shared_ptr<FitnessMetric> m_fitnessMetric = make_shared<ChromosomeLengthMetric>();
};

BOOST_AUTO_TEST_SUITE(Phaser)
BOOST_AUTO_TEST_SUITE(PopulationTest)

BOOST_AUTO_TEST_CASE(isFitter_should_use_fitness_as_the_main_criterion)
{
	BOOST_TEST(isFitter(Individual{Chromosome("a"), 5}, Individual{Chromosome("a"), 10}));
	BOOST_TEST(!isFitter(Individual{Chromosome("a"), 10}, Individual{Chromosome("a"), 5}));

	BOOST_TEST(isFitter(Individual{Chromosome("aaa"), 5}, Individual{Chromosome("aaaaa"), 10}));
	BOOST_TEST(!isFitter(Individual{Chromosome("aaaaa"), 10}, Individual{Chromosome("aaa"), 5}));

	BOOST_TEST(isFitter(Individual{Chromosome("aaaaa"), 5}, Individual{Chromosome("aaa"), 10}));
	BOOST_TEST(!isFitter(Individual{Chromosome("aaa"), 10}, Individual{Chromosome("aaaaa"), 5}));
}

BOOST_AUTO_TEST_CASE(isFitter_should_use_alphabetical_order_when_fitness_is_the_same)
{
	BOOST_TEST(isFitter(Individual{Chromosome("a"), 3}, Individual{Chromosome("c"), 3}));
	BOOST_TEST(!isFitter(Individual{Chromosome("c"), 3}, Individual{Chromosome("a"), 3}));

	BOOST_TEST(isFitter(Individual{Chromosome("a"), 3}, Individual{Chromosome("aa"), 3}));
	BOOST_TEST(!isFitter(Individual{Chromosome("aa"), 3}, Individual{Chromosome("a"), 3}));

	BOOST_TEST(isFitter(Individual{Chromosome("T"), 3}, Individual{Chromosome("a"), 3}));
	BOOST_TEST(!isFitter(Individual{Chromosome("a"), 3}, Individual{Chromosome("T"), 3}));
}

BOOST_AUTO_TEST_CASE(isFitter_should_return_false_for_identical_individuals)
{
	BOOST_TEST(!isFitter(Individual{Chromosome("a"), 3}, Individual{Chromosome("a"), 3}));
	BOOST_TEST(!isFitter(Individual{Chromosome("acT"), 0}, Individual{Chromosome("acT"), 0}));
}

BOOST_FIXTURE_TEST_CASE(constructor_should_copy_chromosomes_and_compute_fitness, PopulationFixture)
{
	vector<Chromosome> chromosomes = {
		Chromosome::makeRandom(5),
		Chromosome::makeRandom(10),
	};
	Population population(m_fitnessMetric, chromosomes);

	BOOST_TEST(population.individuals().size() == 2);
	BOOST_TEST(population.individuals()[0].chromosome == chromosomes[0]);
	BOOST_TEST(population.individuals()[1].chromosome == chromosomes[1]);

	BOOST_TEST(population.individuals()[0].fitness == m_fitnessMetric->evaluate(population.individuals()[0].chromosome));
	BOOST_TEST(population.individuals()[1].fitness == m_fitnessMetric->evaluate(population.individuals()[1].chromosome));
}

BOOST_FIXTURE_TEST_CASE(makeRandom_should_get_chromosome_lengths_from_specified_generator, PopulationFixture)
{
	size_t chromosomeCount = 30;
	size_t maxLength = 5;
	assert(chromosomeCount % maxLength == 0);

	auto nextLength = [counter = 0, maxLength]() mutable { return counter++ % maxLength; };
	auto population = Population::makeRandom(m_fitnessMetric, chromosomeCount, nextLength);

	// We can't rely on the order since the population sorts its chromosomes immediately but
	// we can check the number of occurrences of each length.
	for (size_t length = 0; length < maxLength; ++length)
		BOOST_TEST(
			count_if(
				population.individuals().begin(),
				population.individuals().end(),
				[&length](auto const& individual) { return individual.chromosome.length() == length; }
			) == chromosomeCount / maxLength
		);
}

BOOST_FIXTURE_TEST_CASE(makeRandom_should_get_chromosome_lengths_from_specified_range, PopulationFixture)
{
	auto population = Population::makeRandom(m_fitnessMetric, 100, 5, 10);
	BOOST_TEST(all_of(
		population.individuals().begin(),
		population.individuals().end(),
		[](auto const& individual){ return 5 <= individual.chromosome.length() && individual.chromosome.length() <= 10; }
	));
}

BOOST_FIXTURE_TEST_CASE(makeRandom_should_use_random_chromosome_length, PopulationFixture)
{
	SimulationRNG::reset(1);
	constexpr int populationSize = 200;
	constexpr int minLength = 5;
	constexpr int maxLength = 10;
	constexpr double relativeTolerance = 0.05;

	auto population = Population::makeRandom(m_fitnessMetric, populationSize, minLength, maxLength);
	vector<size_t> samples = chromosomeLengths(population);

	const double expectedValue = (maxLength + minLength) / 2.0;
	const double variance = ((maxLength - minLength + 1) * (maxLength - minLength + 1) - 1) / 12.0;

	BOOST_TEST(abs(mean(samples) - expectedValue) < expectedValue * relativeTolerance);
	BOOST_TEST(abs(meanSquaredError(samples, expectedValue) - variance) < variance * relativeTolerance);
}

BOOST_FIXTURE_TEST_CASE(makeRandom_should_return_population_with_random_chromosomes, PopulationFixture)
{
	SimulationRNG::reset(1);
	constexpr int populationSize = 100;
	constexpr int chromosomeLength = 30;
	constexpr double relativeTolerance = 0.01;

	map<string, size_t> stepIndices = enumerateOptmisationSteps();
	auto population = Population::makeRandom(m_fitnessMetric, populationSize, chromosomeLength, chromosomeLength);

	vector<size_t> samples;
	for (auto& individual: population.individuals())
		for (auto& step: individual.chromosome.optimisationSteps())
			samples.push_back(stepIndices.at(step));

	const double expectedValue = (stepIndices.size() - 1) / 2.0;
	const double variance = (stepIndices.size() * stepIndices.size() - 1) / 12.0;

	BOOST_TEST(abs(mean(samples) - expectedValue) < expectedValue * relativeTolerance);
	BOOST_TEST(abs(meanSquaredError(samples, expectedValue) - variance) < variance * relativeTolerance);
}

BOOST_FIXTURE_TEST_CASE(makeRandom_should_compute_fitness, PopulationFixture)
{
	auto population = Population::makeRandom(m_fitnessMetric, 3, 5, 10);

	BOOST_TEST(population.individuals()[0].fitness == m_fitnessMetric->evaluate(population.individuals()[0].chromosome));
	BOOST_TEST(population.individuals()[1].fitness == m_fitnessMetric->evaluate(population.individuals()[1].chromosome));
	BOOST_TEST(population.individuals()[2].fitness == m_fitnessMetric->evaluate(population.individuals()[2].chromosome));
}

BOOST_FIXTURE_TEST_CASE(run_should_not_make_fitness_of_top_chromosomes_worse, PopulationFixture)
{
	stringstream output;
	vector<Chromosome> chromosomes = {
		Chromosome(vector<string>{StructuralSimplifier::name}),
		Chromosome(vector<string>{BlockFlattener::name}),
		Chromosome(vector<string>{SSAReverser::name}),
		Chromosome(vector<string>{UnusedPruner::name}),
		Chromosome(vector<string>{StructuralSimplifier::name, BlockFlattener::name}),
	};
	Population population(m_fitnessMetric, chromosomes);

	size_t initialTopFitness[2] = {
		m_fitnessMetric->evaluate(chromosomes[0]),
		m_fitnessMetric->evaluate(chromosomes[1]),
	};

	for (int i = 0; i < 6; ++i)
	{
		population.run(1, output);
		BOOST_TEST(population.individuals().size() == 5);

		size_t currentTopFitness[2] = {
			population.individuals()[0].fitness,
			population.individuals()[1].fitness,
		};
		BOOST_TEST(currentTopFitness[0] <= initialTopFitness[0]);
		BOOST_TEST(currentTopFitness[1] <= initialTopFitness[1]);
		BOOST_TEST(currentTopFitness[0] <= currentTopFitness[1]);
	}
}

BOOST_FIXTURE_TEST_CASE(plus_operator_should_add_two_populations, PopulationFixture)
{
	BOOST_CHECK_EQUAL(
		Population(m_fitnessMetric, {Chromosome("ac"), Chromosome("cx")}) +
		Population(m_fitnessMetric, {Chromosome("g"), Chromosome("h"), Chromosome("iI")}),
		Population(m_fitnessMetric, {Chromosome("ac"), Chromosome("cx"), Chromosome("g"), Chromosome("h"), Chromosome("iI")})
	);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

}
