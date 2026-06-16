/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Synergy App Ltd
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "synergy/license/OfflineActivation.h"
#include "synergy/license/parse_serial_key.h"

#include <string>

#include <gtest/gtest.h>

using namespace synergy::license;

// Vectors produced by the vendor signer (web-muskoka offline-activation-protocol.spec.ts,
// fixed secret 0707...07), so these tests prove the client and the server signer implement
// the same wire format.
const auto kVectorSecret = "0707070707070707070707070707070707070707070707070707070707070707";
const auto kVectorMachineId = "test-machine-id-0001";
const auto kVectorSerial = "deadbeef00112233";
const auto kVectorChallenge = "CS34563YJPF9";
const auto kVectorResponse = "FNY0QQP3MM39";

TEST(OfflineActivationTests, buildOfflineChallenge_vectorInputs_matchesVendorTool)
{
  EXPECT_EQ(buildOfflineChallenge(kVectorMachineId, kVectorSerial), kVectorChallenge);
}

TEST(OfflineActivationTests, buildOfflineChallenge_serialCaseAndPaddingDiffer_sameChallenge)
{
  EXPECT_EQ(buildOfflineChallenge(kVectorMachineId, " DEADBEEF00112233 "), kVectorChallenge);
}

TEST(OfflineActivationTests, buildOfflineChallenge_differentMachine_differentChallenge)
{
  EXPECT_NE(buildOfflineChallenge("test-machine-id-0002", kVectorSerial), kVectorChallenge);
}

TEST(OfflineActivationTests, buildOfflineChallenge_differentSerial_differentChallenge)
{
  EXPECT_NE(buildOfflineChallenge(kVectorMachineId, "deadbeef00112234"), kVectorChallenge);
}

TEST(OfflineActivationTests, verifyOfflineResponse_vendorSignedResponse_verifies)
{
  EXPECT_TRUE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, kVectorResponse, kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_groupedLowercaseResponse_verifies)
{
  EXPECT_TRUE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, "fny0-qqp3-mm39", kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_confusableLettersTyped_verifies)
{
  EXPECT_TRUE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, "FNYOQQP3MM39", kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_tamperedResponse_fails)
{
  auto tampered = std::string(kVectorResponse);
  tampered[0] = (tampered[0] == 'C') ? 'D' : 'C';

  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, tampered, kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_differentMachine_fails)
{
  EXPECT_FALSE(verifyOfflineResponse("test-machine-id-0002", kVectorSerial, kVectorResponse, kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_differentSerial_fails)
{
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, "deadbeef00112234", kVectorResponse, kVectorSecret));
}

TEST(OfflineActivationTests, buildOfflineChallenge_emptyMachineId_returnsEmpty)
{
  EXPECT_EQ(buildOfflineChallenge("", kVectorSerial), "");
}

TEST(OfflineActivationTests, verifyOfflineResponse_emptyMachineId_fails)
{
  EXPECT_FALSE(verifyOfflineResponse("", kVectorSerial, kVectorResponse, kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_malformedResponse_fails)
{
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, "", kVectorSecret));
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, "not a code @#!", kVectorSecret));
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, "FNY0", kVectorSecret));
}

TEST(OfflineActivationTests, verifyOfflineResponse_malformedSecret_fails)
{
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, kVectorResponse, ""));
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, kVectorResponse, "abc123"));
}

TEST(OfflineActivationTests, verifyOfflineResponse_embeddedProductionSecret_rejectsTestVector)
{
  EXPECT_FALSE(verifyOfflineResponse(kVectorMachineId, kVectorSerial, kVectorResponse));
}

// Fixed-secret vector asserted identically in the vendor signer's
// offline-activation-protocol.spec.ts (web-muskoka), so this pins both
// implementations to one shared vector.
TEST(OfflineActivationTests, verifyOfflineResponse_sharedVendorVector_verifies)
{
  EXPECT_EQ(buildOfflineChallenge("test machine id", "TEST SERIAL KEY"), "7D6NNQP2VMRH");
  EXPECT_TRUE(verifyOfflineResponse("test machine id", "TEST SERIAL KEY", "PHSD-R6RS-YSSX", kVectorSecret));
}

TEST(OfflineActivationTests, formatOfflineCode_groupsOfFour_insertsDashes)
{
  EXPECT_EQ(formatOfflineCode("AAAABBBBCC", 4), "AAAA-BBBB-CC");
  EXPECT_EQ(formatOfflineCode("AAAA", 4), "AAAA");
  EXPECT_EQ(formatOfflineCode("", 4), "");
}

TEST(OfflineActivationTests, parseSerialKey_offlineMode_isOffline)
{
  // {v3;offline;;business;Bob;1;bob@example.com;Acme;;}
  const auto serialKey = parseSerialKey("7b76333b6f66666c696e653b3b627573696e6573733b426f623b313b626f6240657"
                                        "8616d706c652e636f6d3b41636d653b3b7d");

  EXPECT_TRUE(serialKey.isValid);
  EXPECT_TRUE(serialKey.isOffline);
  EXPECT_EQ(serialKey.seats, 1);
}

TEST(OfflineActivationTests, parseSerialKey_multiSeatKey_parsesSeatCount)
{
  // {v3;offline;;business;Bob;2;bob@example.com;Acme;;}
  const auto serialKey = parseSerialKey("7b76333b6f66666c696e653b3b627573696e6573733b426f623b323b626f6240657"
                                        "8616d706c652e636f6d3b41636d653b3b7d");

  EXPECT_EQ(serialKey.seats, 2);
}
