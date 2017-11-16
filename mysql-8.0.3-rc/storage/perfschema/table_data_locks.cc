/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_data_locks.cc
  Table DATA_LOCKS (implementation).
*/

#include "storage/perfschema/table_data_locks.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "sql/field.h"

THR_LOCK table_data_locks::m_table_lock;

Plugin_table table_data_locks::m_table_def(
  /* Schema name */
  "performance_schema",
  /* Name */
  "data_locks",
  /* Definition */
  "  ENGINE VARCHAR(32) not null,\n"
  "  ENGINE_LOCK_ID VARCHAR(128) not null,\n"
  "  ENGINE_TRANSACTION_ID BIGINT unsigned,\n"
  "  THREAD_ID BIGINT unsigned,\n"
  "  EVENT_ID BIGINT unsigned,\n"
  "  OBJECT_SCHEMA VARCHAR(64),\n"
  "  OBJECT_NAME VARCHAR(64),\n"
  "  PARTITION_NAME VARCHAR(64),\n"
  "  SUBPARTITION_NAME VARCHAR(64),\n"
  "  INDEX_NAME VARCHAR(64),\n"
  "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
  "  LOCK_TYPE VARCHAR(32) not null,\n"
  "  LOCK_MODE VARCHAR(32) not null,\n"
  "  LOCK_STATUS VARCHAR(32) not null,\n"
  "  LOCK_DATA VARCHAR(8192) CHARACTER SET utf8mb4,\n"
  "  PRIMARY KEY (ENGINE_LOCK_ID, ENGINE) USING HASH,\n"
  "  KEY (ENGINE_TRANSACTION_ID, ENGINE) USING HASH,\n"
  "  KEY (THREAD_ID, EVENT_ID) USING HASH,\n"
  "  KEY (OBJECT_SCHEMA, OBJECT_NAME, PARTITION_NAME,\n"
  "       SUBPARTITION_NAME) USING HASH\n",
  /* Options */
  " ENGINE=PERFORMANCE_SCHEMA",
  /* Tablespace */
  nullptr);

PFS_engine_table_share table_data_locks::m_share = {
  &pfs_readonly_acl,
  table_data_locks::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_data_locks::get_row_count,
  sizeof(pk_pos_t),
  &m_table_lock,
  &m_table_def,
  false, /* perpetual */
  PFS_engine_table_proxy(),
  {0},
  false /* m_in_purgatory */
};

PFS_engine_table *
table_data_locks::create(PFS_engine_table_share *)
{
  return new table_data_locks();
}

ha_rows
table_data_locks::get_row_count(void)
{
  // FIXME
  return 99999;
}

table_data_locks::table_data_locks()
  : PFS_engine_table(&m_share, &m_pk_pos),
    m_row(NULL),
    m_pos(),
    m_next_pos(),
    m_pk_pos()
{
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++)
  {
    m_iterator[i] = NULL;
  }
}

table_data_locks::~table_data_locks()
{
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++)
  {
    if (m_iterator[i] != NULL)
    {
      g_data_lock_inspector[i]->destroy_data_lock_iterator(m_iterator[i]);
    }
  }
}

void
table_data_locks::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
  m_pk_pos.reset();
  m_container.clear();
}

int
table_data_locks::rnd_next(void)
{
  row_data_lock *data;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_engine(); m_pos.next_engine())
  {
    unsigned int index = m_pos.m_index_1;

    if (m_iterator[index] == NULL)
    {
      if (g_data_lock_inspector[index] == NULL)
      {
        continue;
      }

      m_iterator[index] =
        g_data_lock_inspector[index]->create_data_lock_iterator();

      if (m_iterator[index] == NULL)
      {
        continue;
      }
    }

    bool iterator_done = false;
    PSI_engine_data_lock_iterator *it = m_iterator[index];

    for (;;)
    {
      data = m_container.get_row(m_pos.m_index_2);
      if (data != NULL)
      {
        m_row = data;
        m_next_pos.set_after(&m_pos);
        m_pk_pos.set(&m_row->m_hidden_pk);
        return 0;
      }

      if (iterator_done)
      {
        break;
      }

      m_container.shrink();
      /*
        TODO: avoid requesting column LOCK_DATA is not used.
      */
      iterator_done = it->scan(&m_container, true);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_data_locks::rnd_pos(const void *pos)
{
  row_data_lock *data;

  set_position(pos);

  /*
    TODO: Multiple engine support.
    Find the proper engine based on column ENGINE.
  */
  static_assert(COUNT_DATA_LOCK_ENGINES == 1,
                "We don't support multiple engines yet.");
  unsigned int index = 0;

  if (m_iterator[index] == NULL)
  {
    if (g_data_lock_inspector[index] == NULL)
    {
      return HA_ERR_RECORD_DELETED;
    }

    m_iterator[index] =
      g_data_lock_inspector[index]->create_data_lock_iterator();

    if (m_iterator[index] == NULL)
    {
      return HA_ERR_RECORD_DELETED;
    }
  }

  PSI_engine_data_lock_iterator *it = m_iterator[index];

  m_container.clear();
  /*
    TODO: avoid requesting column LOCK_DATA is not used.
  */
  it->fetch(&m_container,
            m_pk_pos.m_engine_lock_id,
            m_pk_pos.m_engine_lock_id_length,
            true);
  data = m_container.get_row(0);
  if (data != NULL)
  {
    m_row = data;
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_data_locks::index_init(uint idx, bool)
{
  PFS_index_data_locks *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_data_locks_by_lock_id);
    break;
  case 1:
    result = PFS_NEW(PFS_index_data_locks_by_transaction_id);
    break;
  case 2:
    result = PFS_NEW(PFS_index_data_locks_by_thread_id);
    break;
  case 3:
    result = PFS_NEW(PFS_index_data_locks_by_object);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  m_opened_index = result;
  m_index = result;

  m_container.set_filter(m_opened_index);
  return 0;
}

int
table_data_locks::index_next()
{
  return rnd_next();
}

int
table_data_locks::read_row_values(TABLE *table,
                                  unsigned char *buf,
                                  Field **fields,
                                  bool read_all)
{
  Field *f;

  if (unlikely(m_row == NULL))
  {
    return HA_ERR_RECORD_DELETED;
  }

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 2);
  buf[0] = 0;
  buf[1] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* ENGINE */
        set_field_varchar_utf8(f, m_row->m_engine);
        break;
      case 1: /* ENGINE_LOCK_ID */
        set_field_varchar_utf8(f,
                               m_row->m_hidden_pk.m_engine_lock_id,
                               m_row->m_hidden_pk.m_engine_lock_id_length);
        break;
      case 2: /* ENGINE_TRANSACTION_ID */
        if (m_row->m_transaction_id != 0)
        {
          set_field_ulonglong(f, m_row->m_transaction_id);
        }
        else
        {
          f->set_null();
        }
        break;
      case 3: /* THREAD_ID */
        if (m_row->m_thread_id != 0)
        {
          set_field_ulonglong(f, m_row->m_thread_id);
        }
        else
        {
          f->set_null();
        }
        break;
      case 4: /* EVENT_ID */
        if (m_row->m_event_id != 0)
        {
          set_field_ulonglong(f, m_row->m_event_id);
        }
        else
        {
          f->set_null();
        }
        break;
      case 5: /* OBJECT_SCHEMA */
        m_row->m_index_row.set_field(1, f);
        break;
      case 6: /* OBJECT_NAME  */
        m_row->m_index_row.set_field(2, f);
        break;
      case 7: /* PARTITION_NAME */
        if (m_row->m_partition_name_length > 0)
        {
          set_field_varchar_utf8(
            f, m_row->m_partition_name, m_row->m_partition_name_length);
        }
        else
        {
          f->set_null();
        }
        break;
      case 8: /* SUBPARTITION_NAME */
        if (m_row->m_sub_partition_name_length > 0)
        {
          set_field_varchar_utf8(
            f, m_row->m_sub_partition_name, m_row->m_sub_partition_name_length);
        }
        else
        {
          f->set_null();
        }
        break;
      case 9: /* INDEX_NAME */
        m_row->m_index_row.set_field(3, f);
        break;
      case 10: /* OBJECT_INSTANCE_BEGIN */
        set_field_ulonglong(f, (intptr)m_row->m_identity);
        break;
      case 11: /* LOCK_TYPE */
        set_field_varchar_utf8(f, m_row->m_lock_type);
        break;
      case 12: /* LOCK_MODE */
        set_field_varchar_utf8(f, m_row->m_lock_mode);
        break;
      case 13: /* LOCK_STATUS */
        set_field_varchar_utf8(f, m_row->m_lock_status);
        break;
      case 14: /* LOCK_DATA */
        if (m_row->m_lock_data != NULL)
        {
          set_field_varchar_utf8mb4(f, m_row->m_lock_data);
        }
        else
        {
          f->set_null();
        }
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
