/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2016 - 2026 Synergy App Ltd
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

#pragma once

#include <stdexcept>
#include <string>

class Product
{
  friend bool operator==(Product const &, Product const &) = default;

public:
  class InvalidProductEdition : public std::runtime_error
  {
  public:
    explicit InvalidProductEdition() : std::runtime_error("invalid product edition")
    {
    }
  };

  class InvalidFeature : public std::runtime_error
  {
  public:
    explicit InvalidFeature() : std::runtime_error("invalid feature")
    {
    }
  };

  enum class Edition
  {
    kUnregistered = -1,
    kBasic = 0,
    kPro = 1,
    kBusiness = 4,
  };

  enum class Feature
  {
    kTls = 0,
    kSettingsScope = 2,
  };

  /**
   * @brief Product edition IDs found in a decoded serial key.
   */
  class SerialKeyEditionID
  {
  public:
    static const std::string Basic;
    static const std::string Pro;
    static const std::string Business;
  };

  Product() = default;
  explicit Product(Edition edition);
  explicit Product(const std::string &serialKeyEditionID);

  bool isValid() const;
  Edition edition() const;
  std::string serialKeyId() const;
  std::string name() const;
  bool isFeatureAvailable(Feature feature) const;

  void setEdition(Edition type);
  void setEdition(const std::string &serialKeyId);

private:
  bool isTlsAvailable() const;
  bool isSettingsScopeAvailable() const;

  Edition m_edition = Edition::kUnregistered;
};
