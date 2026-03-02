// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __sa_Diagnostic_h
#define __sa_Diagnostic_h

#include "Severity.h"
#include "Category.h"
#include "Location.h"
#include "Lockable.h"
#include "base/Chain.h"
#include "base/RefCounted.h"
#include "base/ptr.h"
#include "base/debug.h"
#include <iostream>

namespace sa {
  class Unit;
  class File;
  class Project;

  class Diagnostic: public base::RefCounted, public Lockable,
                    public base::Chain<Diagnostic>
  {

  public:
    const std::string message;
    const Severity severity;
    const Category category;
    const base::ptr<File> file;
    const Location location;

    // Create a diagnostic with a location in a file
    Diagnostic(
      const std::string &message,
      Severity severity,
      Category category,
      File *file,
      const Location &location
    );

    // Create a project diagnostic without location
    Diagnostic(
      const std::string &message,
      Severity severity,
      Category category,
      Project *project
    );

    // Create a diagnostic chain.
    Diagnostic();

    // Destroy a diagnostic. Do not call directly if reference counting is used.
    ~Diagnostic();
    
    const std::string &get_message() const { return message; }

    Severity get_severity() const { return severity; }

    Category get_category() const { return category; }

    void *get_user_data() const { return _user_data; }

    // A compilation diagnostic can be visible as long as it is instantiated at
    // least once, i.e. as long as its instantiation count is greater than zero.
    // Whether an instantiated diagnostic is actually visible is subject to
    // the maximum number of diagnostics that can be visible at the same time.
    bool is_instantiated() const { return _instantiation_count; }

    // Increment this diagnostic's instantiation count.
    void include_instance();

    // Decrement this diagnostic's instantiation count.
    void exclude_instance();

    // Set the user data for this diagnostic. Set this to a value by which the
    // application can identify the diagnostic. When the diagnostic's
    // instantiation count is decremented to zero, the user data will be passed
    // to the application callback function to remove the diagnostic.
    void set_user_data(void *user_data);

    // Flag used by the Project to efficiently manage diagnostics.  There is a
    // maximum number of diagnostics that can be visible at the same time.
    // Only instantiated diagnostics not above that maximum are visible.
    bool is_visible = false;

#ifndef NDEBUG
    std::string get_debug_name() const override;
#endif

  private:
    Project *const project;
    size_t _instantiation_count = 0;
    void *_user_data = 0;
  };

  std::ostream &operator<<(std::ostream &out, const Diagnostic &diagnostic);

  typedef void (*DiagnosticReporter)(
    const std::string &message,
    Severity severity,
    Category category
  );
}

#endif
  
