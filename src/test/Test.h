#pragma once

// unit test class

#include <cppunit/extensions/HelperMacros.h>

class Test : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( Test );
    CPPUNIT_TEST( testUserStatus );
    CPPUNIT_TEST( testUserBattleStatus );
    CPPUNIT_TEST( testUser );
    CPPUNIT_TEST( testBattle );
    CPPUNIT_TEST( testScript );
    CPPUNIT_TEST( testBot );
    CPPUNIT_TEST( testMyImage );
    CPPUNIT_TEST_SUITE_END();

 public:
    void setUp();
    void tearDown();

    void testUserStatus();
    void testUserBattleStatus();
    void testUser();
    void testBattle();
    void testScript();
    void testBot();
    void testMyImage();

};