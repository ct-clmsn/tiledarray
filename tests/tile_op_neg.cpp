/*
 *  This file is a part of TiledArray.
 *  Copyright (C) 2013  Virginia Tech
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Justus Calvin
 *  Department of Chemistry, Virginia Tech
 *
 *  tile_op_neg.cpp
 *  May 9, 2013
 *
 */

#include "TiledArray/tile_op/neg.h"
#include "unit_test_config.h"
#include "TiledArray/tensor.h"
#include "range_fixture.h"

using namespace TiledArray;

struct NegFixture : public RangeFixture {

  NegFixture() :
    a(RangeFixture::r),
    b(),
    perm(2,0,1)
  {
    GlobalFixture::world->srand(27);
    for(std::size_t i = 0ul; i < r.volume(); ++i) {
      a[i] = GlobalFixture::world->rand() / 101;
    }
  }

  ~NegFixture() { }

  Tensor<int> a;
  Tensor<int> b;
  Permutation perm;

}; // NegFixture

BOOST_FIXTURE_TEST_SUITE( tile_op_neg_suite, NegFixture )

BOOST_AUTO_TEST_CASE( constructor )
{
  // Check that the constructors can be called without throwing exceptions
  BOOST_CHECK_NO_THROW((math::Neg<Tensor<int>, Tensor<int>, false>()));
  BOOST_CHECK_NO_THROW((math::Neg<Tensor<int>, Tensor<int>, false>(perm)));
  BOOST_CHECK_NO_THROW((math::Neg<Tensor<int>, Tensor<int>, true>()));
  BOOST_CHECK_NO_THROW((math::Neg<Tensor<int>, Tensor<int>, true>(perm)));
}

BOOST_AUTO_TEST_CASE( unary_neg )
{
  math::Neg<Tensor<int>, Tensor<int>, false> neg_op;

  // Store the sum of a and b in c
  BOOST_CHECK_NO_THROW(b = neg_op(a));

  // Check that the result range is correct
  BOOST_CHECK_EQUAL(b.range(), a.range());

  // Check that a nor b were consumed
  BOOST_CHECK_NE(b.data(), a.data());

  // Check that the data in the new tile is correct
  for(std::size_t i = 0ul; i < r.volume(); ++i) {
    BOOST_CHECK_EQUAL(b[i], -a[i]);
  }
}

BOOST_AUTO_TEST_CASE( unary_neg_perm )
{
  math::Neg<Tensor<int>, Tensor<int>, false> neg_op(perm);

  // Store the sum of a and b in c
  BOOST_CHECK_NO_THROW(b = neg_op(a));

  // Check that the result range is correct
  BOOST_CHECK_EQUAL(b.range(), a.range());

  // Check that a nor b were consumed
  BOOST_CHECK_NE(b.data(), a.data());

  // Check that the data in the new tile is correct
  for(std::size_t i = 0ul; i < r.volume(); ++i) {
    BOOST_CHECK_EQUAL(b[perm ^ a.range().idx(i)], -a[i]);
  }
}

BOOST_AUTO_TEST_CASE( unary_neg_consume )
{
  math::Neg<Tensor<int>, Tensor<int>, true> neg_op;
  const Tensor<int> ax(a.range(), a.begin());

  // Store the sum of a and b in c
  BOOST_CHECK_NO_THROW(b = neg_op(a));

  // Check that the result range is correct
  BOOST_CHECK_EQUAL(b.range(), a.range());

  // Check that a nor b were consumed
  BOOST_CHECK_EQUAL(b.data(), a.data());

  // Check that the data in the new tile is correct
  for(std::size_t i = 0ul; i < r.volume(); ++i) {
    BOOST_CHECK_EQUAL(b[i], -ax[i]);
  }
}


BOOST_AUTO_TEST_CASE( unary_neg_perm_consume )
{
  math::Neg<Tensor<int>, Tensor<int>, true> neg_op(perm);

  // Store the sum of a and b in c
  BOOST_CHECK_NO_THROW(b = neg_op(a));

  // Check that the result range is correct
  BOOST_CHECK_EQUAL(b.range(), a.range());

  // Check that a nor b were consumed
  BOOST_CHECK_NE(b.data(), a.data());

  // Check that the data in the new tile is correct
  for(std::size_t i = 0ul; i < r.volume(); ++i) {
    BOOST_CHECK_EQUAL(b[perm ^ a.range().idx(i)], -a[i]);
  }
}


BOOST_AUTO_TEST_SUITE_END()
