/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RESOURCEGROUPS_RESOURCE_GROUP_SQL_CMD_H_
#define RESOURCEGROUPS_RESOURCE_GROUP_SQL_CMD_H_

#include "sql/parse_tree_node_base.h"        // Trivial_array
#include "sql/resourcegroups/platform/thread_attrs_api.h"
#include "sql/resourcegroups/resource_group_basic_types.h" // Type, Range
#include "sql/sql_base.h"
#include "sql/sql_cmd.h"
#include "sql/sql_cmd_dml.h"


#include <memory>
#include <vector>

class PT_alter_resource_group;
class PT_create_resource_group;
class PT_drop_resource_group;
class PT_set_resource_group;

namespace resourcegroups
{

class Resource_group;


/**
  Sql_cmd_create_resource_group represents CREATE RESOURCE GROUP statement.
*/

class Sql_cmd_create_resource_group : public Sql_cmd
{
  friend class ::PT_create_resource_group;

public:
  Sql_cmd_create_resource_group(const LEX_CSTRING &name, const Type type,
                                const Trivial_array<Range> *cpu_list,
                                int priority, bool enabled)
    : m_name(name), m_type(type), m_cpu_list(cpu_list), m_priority(priority),
      m_enabled(enabled)
  {}

  enum_sql_command sql_command_code() const override
  { return SQLCOM_CREATE_RESOURCE_GROUP; }

  bool execute(THD *thd) override;

private:
  const LEX_CSTRING m_name;
  const Type m_type;
  const Trivial_array<Range> *m_cpu_list;
  int m_priority;
  bool m_enabled;
};


/**
  Sql_cmd_alter_resource_group represents ALTER RESOURCE GROUP statement.
*/

class Sql_cmd_alter_resource_group : public Sql_cmd
{
  friend class ::PT_alter_resource_group;

public:
  Sql_cmd_alter_resource_group(const LEX_CSTRING& name,
                               const Trivial_array<Range> *cpu_list,
                               int priority, bool enable, bool force,
                               bool use_enable)
    : m_name(name), m_cpu_list(cpu_list), m_priority(priority),
      m_enable(enable), m_force(force), m_use_enable(use_enable)
  {}

  enum_sql_command sql_command_code() const override
  { return SQLCOM_ALTER_RESOURCE_GROUP; }

  bool execute(THD *thd) override;

private:
  const LEX_CSTRING m_name;
  const Trivial_array<Range> *m_cpu_list;
  int m_priority;
  bool m_enable;
  bool m_force;
  bool m_use_enable;
};


/**
  Sql_cmd_drop_resource_group represents DROP RESOURCE GROUP statement.
*/

class Sql_cmd_drop_resource_group : public Sql_cmd
{
  friend class ::PT_drop_resource_group;

public:
  Sql_cmd_drop_resource_group(const LEX_CSTRING& name, bool force)
    : m_name(name), m_force(force)
  {}

  enum_sql_command sql_command_code() const override
  { return SQLCOM_DROP_RESOURCE_GROUP; }

  bool execute(THD *thd) override;

private:
  const LEX_CSTRING m_name;
  bool m_force;
};


/**
  Sql_cmd_set_resource_group represents SET RESOURCE GROUP statement.
*/

class Sql_cmd_set_resource_group : public Sql_cmd
{
  friend class ::PT_set_resource_group;

public:

  Sql_cmd_set_resource_group(const LEX_CSTRING &name,
                             Trivial_array<ulonglong> *thread_id_list)
    : m_name(name), m_thread_id_list(thread_id_list)
  {}

  enum_sql_command sql_command_code() const override
  { return SQLCOM_SET_RESOURCE_GROUP; }

  bool execute(THD *thd) override;

private:
  const LEX_CSTRING m_name;
  Trivial_array<ulonglong> *m_thread_id_list;
};
} // resourcegroups
#endif // RESOURCEGROUPS_RESOURCE_GROUP_SQL_CMD_H_
