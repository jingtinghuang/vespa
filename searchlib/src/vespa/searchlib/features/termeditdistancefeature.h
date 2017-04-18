// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include <string>
#include <vector>
#include <vespa/searchlib/fef/blueprint.h>
#include <vespa/searchlib/fef/featureexecutor.h>
#include <vespa/searchlib/common/feature.h>

namespace search {
namespace features {

/**
 * Implements a cell class for the cost table constructed when running the term edit distance calculator. This is
 * necessary to keep track of the route actually chosen through the table, since the algorithm itself merely find the
 * minimum cost.
 */
class TedCell {
public:
    TedCell();
    TedCell(feature_t cost, uint32_t numDel, uint32_t numIns, uint32_t numSub);

    feature_t cost;   // The cost at this point.
    uint32_t  numDel; // The number of deletions to get here.
    uint32_t  numIns; // The number of insertions to get here.
    uint32_t  numSub; // The number of substitutions to get here.
};

/**
 * Implements the necessary config for the term edit distance calculator. This class exists so that the executor does
 * not need a separate copy of the config parsed by the blueprint, and at the same time avoiding that the executor needs
 * to know about the blueprint.
 */
struct TermEditDistanceConfig {
    TermEditDistanceConfig();

    uint32_t  fieldId;    // The id of field to process.
    uint32_t  fieldBegin; // The first field term to evaluate.
    uint32_t  fieldEnd;   // The last field term to evaluate.
    feature_t costDel;    // The cost of a delete.
    feature_t costIns;    // The cost of an insert.
    feature_t costSub;    // The cost of a substitution.
};

/**
 * Implements the executor for the term edit distance calculator.
 */
class TermEditDistanceExecutor : public search::fef::FeatureExecutor {
public:
    /**
     * Constructs a new executor for the term edit distance calculator.
     *
     * @param config The config for this executor.
     */
    TermEditDistanceExecutor(const search::fef::IQueryEnvironment &env,
                             const TermEditDistanceConfig &config);


    /**
     *
     * This executor prepares a matrix that has one row per query term, and one column per field term. Initialize this
     * array as follows:
     *
     *   |f i e l d
     *  -+---------
     *  q|0 1 2 3 4
     *  u|1 . . . .
     *  e|2 . . . .
     *  r|3 . . . .
     *  y|4 . . . .
     *
     * Run through this matrix per field term, per query term; i.e. column by column, row by row. Compare the field term
     * at that column with the query term at that row. Then set the value of that cell to the minimum of:
     *
     * 1. The cost of substitution; the above-left value plus the cost (0 if equal).
     * 2. The cost of insertion; the left value plus the cost.
     * 3. The cost of deletion; the above value plus the cost.
     *
     * After completing the matrix, the minimum cost is contained in the bottom-right.
     *
     * @param docid local document id to be evaluated
     */
    virtual void execute(uint32_t docId) override;

private:
    /**
     * Writes the given list of feature values to log so that it can be viewed for instrumentation.
     *
     * @param row     The list of feature values to write.
     * @param numCols The number of columns to write.
     */
    void logRow(const std::vector<TedCell> &row, size_t numCols);

    virtual void handle_bind_match_data(fef::MatchData &md) override;

private:
    const TermEditDistanceConfig             &_config;       // The config for this executor.
    std::vector<search::fef::TermFieldHandle> _fieldHandles; // The handles of all query terms.
    std::vector<feature_t>                    _termWeights;  // The weights of all query terms.
    std::vector<TedCell>                      _prevRow;      // Optimized representation of the cost table.
    std::vector<TedCell>                      _thisRow;      //
    const fef::MatchData                     *_md;
};

/**
 * Implements the blueprint for the term edit distance calculator.
 */
class TermEditDistanceBlueprint : public search::fef::Blueprint {
public:
    /**
     * Constructs a new blueprint for the term edit distance calculator.
     */
    TermEditDistanceBlueprint();

    // Inherit doc from Blueprint.
    virtual void visitDumpFeatures(const search::fef::IIndexEnvironment &env,
                                   search::fef::IDumpFeatureVisitor &visitor) const override;

    // Inherit doc from Blueprint.
    virtual search::fef::Blueprint::UP createInstance() const override;

    // Inherit doc from Blueprint.
    virtual search::fef::FeatureExecutor &createExecutor(const search::fef::IQueryEnvironment &env, vespalib::Stash &stash) const override;

    // Inherit doc from Blueprint.
    virtual search::fef::ParameterDescriptions getDescriptions() const override {
        return search::fef::ParameterDescriptions().desc().indexField(search::fef::ParameterCollection::SINGLE);
    }

    /**
     * The cost of each operation is specified by the parameters to the {@link #setup} method of this blueprint. All
     * costs are multiplied by the relative weight of eacht query term. Furthermore, if the query term is not mandatory,
     * all operations are free. The parameters are:
     *
     * 1. The name of the field to calculate the distance for.
     * 2. The cost of ignoring a query term, this is typically HIGH.
     * 3. The cost of inserting a field term into the query term, this is typically LOW.
     * 4. The cost of substituting a field term with a query term, this is also typically LOW.
     * 5. Optional: The field position to begin iteration.
     * 6. Optional: The field position to end iteration.
     *
     * @param env    The index environment.
     * @param params A list of the parameters mentioned above.
     * @return Whether or not setup was possible.
     */
    virtual bool setup(const search::fef::IIndexEnvironment & env,
                       const search::fef::ParameterList & params) override;

private:
    TermEditDistanceConfig _config; // The config for this blueprint.
};

}}

