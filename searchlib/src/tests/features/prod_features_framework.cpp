// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/fastos/fastos.h>
#include <vespa/log/log.h>
LOG_SETUP(".prod_features_framework");

#include "prod_features.h"
#include <vespa/searchlib/features/valuefeature.h>

using namespace search::features;
using namespace search::fef;
using namespace search::fef::test;
using CollectionType = FieldInfo::CollectionType;

void
Test::testFramework()
{
    LOG(info, "testFramework()");
    IndexEnvironment indexEnv;
    { // test index environment builder
        IndexEnvironmentBuilder ieb(indexEnv);
        ieb.addField(FieldType::INDEX, CollectionType::SINGLE, "foo")
            .addField(FieldType::ATTRIBUTE, CollectionType::WEIGHTEDSET, "bar")
            .addField(FieldType::INDEX, CollectionType::ARRAY, "baz");
        {
            const FieldInfo * info = indexEnv.getFieldByName("foo");
            ASSERT_TRUE(info != NULL);
            EXPECT_EQUAL(info->id(), 0u);
            EXPECT_TRUE(info->type() == FieldType::INDEX);
            EXPECT_TRUE(info->collection() == CollectionType::SINGLE);
        }
        {
            const FieldInfo * info = indexEnv.getFieldByName("bar");
            ASSERT_TRUE(info != NULL);
            EXPECT_EQUAL(info->id(), 1u);
            EXPECT_TRUE(info->type() == FieldType::ATTRIBUTE);
            EXPECT_TRUE(info->collection() == CollectionType::WEIGHTEDSET);
        }
        {
            const FieldInfo * info = indexEnv.getFieldByName("baz");
            ASSERT_TRUE(info != NULL);
            EXPECT_EQUAL(info->id(), 2u);
            EXPECT_TRUE(info->type() == FieldType::INDEX);
            EXPECT_TRUE(info->collection() == CollectionType::ARRAY);
        }
        ASSERT_TRUE(indexEnv.getFieldByName("qux") == NULL);
    }

    QueryEnvironment queryEnv(&indexEnv);
    MatchDataLayout layout;
    { // test query environment builder
        QueryEnvironmentBuilder qeb(queryEnv, layout);
        {
            SimpleTermData &tr = qeb.addAllFields();
            ASSERT_TRUE(tr.lookupField(0) != 0);
            ASSERT_TRUE(tr.lookupField(1) != 0);
            ASSERT_TRUE(tr.lookupField(2) != 0);
            EXPECT_TRUE(tr.lookupField(3) == 0);
            EXPECT_TRUE(tr.lookupField(0)->getHandle() == 0u);
            EXPECT_TRUE(tr.lookupField(1)->getHandle() == 1u);
            EXPECT_TRUE(tr.lookupField(2)->getHandle() == 2u);
            const ITermData *tp = queryEnv.getTerm(0);
            ASSERT_TRUE(tp != NULL);
            EXPECT_EQUAL(tp, &tr);
        }
        {
            SimpleTermData *tr = qeb.addAttributeNode("bar");
            ASSERT_TRUE(tr != 0);
            ASSERT_TRUE(tr->lookupField(1) != 0);
            EXPECT_TRUE(tr->lookupField(0) == 0);
            EXPECT_TRUE(tr->lookupField(2) == 0);
            EXPECT_TRUE(tr->lookupField(3) == 0);
            EXPECT_TRUE(tr->lookupField(1)->getHandle() == 3u);
            const ITermData *tp = queryEnv.getTerm(1);
            ASSERT_TRUE(tp != NULL);
            EXPECT_EQUAL(tp, tr);
        }
    }

    MatchData::UP data = layout.createMatchData();
    EXPECT_EQUAL(data->getNumTermFields(), 4u);

    { // check match data access
        MatchDataBuilder mdb(queryEnv, *data);

        // setup some occurence lists
        ASSERT_TRUE(mdb.addOccurence("foo", 0, 20));
        ASSERT_TRUE(mdb.addOccurence("foo", 0, 10));
        ASSERT_TRUE(mdb.setFieldLength("foo", 50));
        ASSERT_TRUE(mdb.addOccurence("baz", 0, 15));
        ASSERT_TRUE(mdb.addOccurence("baz", 0, 5));
        ASSERT_TRUE(mdb.setFieldLength("baz", 100));
        ASSERT_TRUE(mdb.apply(100));

        {
            {
                TermFieldMatchData *tfmd = mdb.getTermFieldMatchData(0, 0);
                ASSERT_TRUE(tfmd != NULL);

                FieldPositionsIterator itr = tfmd->getIterator(); // foo (index)
                ASSERT_TRUE(itr.valid());
                EXPECT_EQUAL(itr.getFieldLength(), 50u);
                EXPECT_EQUAL(itr.getPosition(), 10u);
                itr.next();
                ASSERT_TRUE(itr.valid());
                EXPECT_EQUAL(itr.getPosition(), 20u);
                itr.next();
                ASSERT_TRUE(!itr.valid());
            }
            {
                TermFieldMatchData *tfmd = mdb.getTermFieldMatchData(0, 1);
                ASSERT_TRUE(tfmd != NULL);

                FieldPositionsIterator itr = tfmd->getIterator(); // bar (attribute)
                ASSERT_TRUE(!itr.valid());
            }
            {
                TermFieldMatchData *tfmd = mdb.getTermFieldMatchData(0, 2);
                ASSERT_TRUE(tfmd != NULL);

                FieldPositionsIterator itr = tfmd->getIterator(); // baz (index)
                ASSERT_TRUE(itr.valid());
                EXPECT_EQUAL(itr.getFieldLength(), 100u);
                EXPECT_EQUAL(itr.getPosition(), 5u);
                itr.next();
                ASSERT_TRUE(itr.valid());
                EXPECT_EQUAL(itr.getPosition(), 15u);
                itr.next();
                ASSERT_TRUE(!itr.valid());
            }
        }
        {
            TermFieldMatchData *tfmd = mdb.getTermFieldMatchData(1, 1);
            ASSERT_TRUE(tfmd != NULL);

            FieldPositionsIterator itr = tfmd->getIterator(); // bar (attribute)
            ASSERT_TRUE(!itr.valid());
        }
    }
    { // check that data is cleared
        MatchDataBuilder mdb(queryEnv, *data);
        EXPECT_EQUAL(mdb.getTermFieldMatchData(0, 0)->getDocId(), TermFieldMatchData::invalidId());
        EXPECT_EQUAL(mdb.getTermFieldMatchData(0, 1)->getDocId(), TermFieldMatchData::invalidId());
        EXPECT_EQUAL(mdb.getTermFieldMatchData(0, 2)->getDocId(), TermFieldMatchData::invalidId());
        EXPECT_EQUAL(mdb.getTermFieldMatchData(1, 1)->getDocId(), TermFieldMatchData::invalidId());

        // test illegal things
        ASSERT_TRUE(!mdb.addOccurence("foo", 1, 10)); // invalid term/field combination
    }

    BlueprintFactory factory;
    factory.addPrototype(Blueprint::SP(new ValueBlueprint()));
    Properties overrides;

    { // test feature test runner
        FeatureTest ft(factory, indexEnv, queryEnv, layout,
                       StringList().add("value(10)").add("value(20)").add("value(30)"), overrides);
        MatchDataBuilder::UP mdb1 = ft.createMatchDataBuilder();
        EXPECT_TRUE(mdb1.get() == NULL);
        EXPECT_TRUE(!ft.execute(RankResult().addScore("value(10)", 10.0f)));
        ASSERT_TRUE(ft.setup());
        MatchDataBuilder::UP mdb2 = ft.createMatchDataBuilder();
        EXPECT_TRUE(mdb2.get() != NULL);

        EXPECT_TRUE(ft.execute(RankResult().addScore("value(10)", 10.0f).addScore("value(20)", 20.0f)));
        EXPECT_TRUE(!ft.execute(RankResult().addScore("value(10)", 20.0f)));
        EXPECT_TRUE(!ft.execute(RankResult().addScore("value(5)", 5.0f)));
    }
    { // test simple constructor
        MatchDataLayout mdl; // match data layout cannot be reused
        FeatureTest ft(factory, indexEnv, queryEnv, mdl, "value(10)", overrides);
        ASSERT_TRUE(ft.setup());
        EXPECT_TRUE(ft.execute(10.0f));
    }
}
