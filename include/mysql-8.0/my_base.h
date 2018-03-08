// Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved. 
// 
// This program is free software; you can redistribute it and/or modify 
// it under the terms of the GNU General Public License, version 2.0, as 
// published by the Free Software Foundation. 
// 
// This program is also distributed with certain software (including 
// but not limited to OpenSSL) that is licensed under separate terms, 
// as designated in a particular file or component or in included license 
// documentation. The authors of MySQL hereby grant you an 
// additional permission to link the program and your derivative works 
// with the separately licensed software that they have included with 
// MySQL. 
// 
// Without limiting anything contained in the foregoing, this file, 
// which is part of MySQL Server, is also subject to the 
// Universal FOSS Exception, version 1.0, a copy of which can be found at 
// http://oss.oracle.com/licenses/universal-foss-exception. 
// 
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
// See the GNU General Public License, version 2.0, for more details. 
// 
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation, Inc., 
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA 
#define HA_ERR_LOCK_WAIT_TIMEOUT 146
#define HA_ERR_LOCK_TABLE_FULL 147
#define HA_ERR_READ_ONLY_TRANSACTION 148 /* Updates not allowed */
#define HA_ERR_LOCK_DEADLOCK 149
#define HA_ERR_CANNOT_ADD_FOREIGN 150    /* Cannot add a foreign key constr. */
#define HA_ERR_NO_REFERENCED_ROW 151     /* Cannot add a child row */
#define HA_ERR_ROW_IS_REFERENCED 152     /* Cannot delete a parent row */
#define HA_ERR_NO_SAVEPOINT 153          /* No savepoint with that name */
#define HA_ERR_NON_UNIQUE_BLOCK_SIZE 154 /* Non unique key block size */
#define HA_ERR_NO_SUCH_TABLE 155 /* The table does not exist in engine */
#define HA_ERR_TABLE_EXIST 156   /* The table existed in storage engine */
#define HA_ERR_NO_CONNECTION 157 /* Could not connect to storage engine */
/* NULLs are not supported in spatial index */
#define HA_ERR_NULL_IN_SPATIAL 158
#define HA_ERR_TABLE_DEF_CHANGED 159 /* The table changed in storage engine */
/* There's no partition in table for given value */
#define HA_ERR_NO_PARTITION_FOUND 160
#define HA_ERR_RBR_LOGGING_FAILED 161 /* Row-based binlogging of row failed */
#define HA_ERR_DROP_INDEX_FK 162      /* Index needed in foreign key constr */
/*
  Upholding foreign key constraints would lead to a duplicate key error
  in some other table.
*/
#define HA_ERR_FOREIGN_DUPLICATE_KEY 163
/* The table changed in storage engine */
#define HA_ERR_TABLE_NEEDS_UPGRADE 164
#define HA_ERR_TABLE_READONLY 165 /* The table is not writable */

#define HA_ERR_AUTOINC_READ_FAILED 166 /* Failed to get next autoinc value */
#define HA_ERR_AUTOINC_ERANGE 167      /* Failed to set row autoinc value */
#define HA_ERR_GENERIC 168             /* Generic error */
/* row not actually updated: new values same as the old values */
#define HA_ERR_RECORD_IS_THE_SAME 169
/* It is not possible to log this statement */
#define HA_ERR_LOGGING_IMPOSSIBLE       \
  170 /* It is not possible to log this \
         statement */
#define HA_ERR_CORRUPT_EVENT                                     \
  171                       /* The event was corrupt, leading to \
                               illegal data being read */
#define HA_ERR_NEW_FILE 172 /* New file format */
#define HA_ERR_ROWS_EVENT_APPLY                                       \
  173                             /* The event could not be processed \
                                     no other hanlder error happened */
#define HA_ERR_INITIALIZATION 174 /* Error during initialization */
#define HA_ERR_FILE_TOO_SHORT 175 /* File too short */
#define HA_ERR_WRONG_CRC 176      /* Wrong CRC on page */
#define HA_ERR_TOO_MANY_CONCURRENT_TRXS \
  177 /*Too many active concurrent transactions */
/* There's no explicitly listed partition in table for the given value */
#define HA_ERR_NOT_IN_LOCK_PARTITIONS 178
#define HA_ERR_INDEX_COL_TOO_LONG 179 /* Index column length exceeds limit */
#define HA_ERR_INDEX_CORRUPT 180      /* InnoDB index corrupted */
#define HA_ERR_UNDO_REC_TOO_BIG 181   /* Undo log record too big */
#define HA_FTS_INVALID_DOCID 182      /* Invalid InnoDB Doc ID */
#define HA_ERR_TABLE_IN_FK_CHECK               \
  183 /* Table being used in foreign key check \
       */
#define HA_ERR_TABLESPACE_EXISTS \
  184 /* The tablespace existed in storage engine */
#define HA_ERR_TOO_MANY_FIELDS 185        /* Table has too many columns */
#define HA_ERR_ROW_IN_WRONG_PARTITION 186 /* Row in wrong partition */
#define HA_ERR_INNODB_READ_ONLY 187       /* InnoDB is in read only mode. */
#define HA_ERR_FTS_EXCEED_RESULT_CACHE_LIMIT \
  188 /* FTS query exceeds result cache limit */
#define HA_ERR_TEMP_FILE_WRITE_FAILURE 189 /* Temporary file write failure */
#define HA_ERR_INNODB_FORCED_RECOVERY     \
  190 /* Innodb is in force recovery mode \
       */
#define HA_ERR_FTS_TOO_MANY_WORDS_IN_PHRASE                         \
  191                                 /* Too many words in a phrase \
                                       */
#define HA_ERR_FK_DEPTH_EXCEEDED 192  /* FK cascade depth exceeded */
#define HA_MISSING_CREATE_OPTION 193  /* Option Missing during Create */
#define HA_ERR_SE_OUT_OF_MEMORY 194   /* Out of memory in storage engine */
#define HA_ERR_TABLE_CORRUPT 195      /* Table/Clustered index is corrupted. */
#define HA_ERR_QUERY_INTERRUPTED 196  /* The query was interrupted */
#define HA_ERR_TABLESPACE_MISSING 197 /* Missing Tablespace */
#define HA_ERR_TABLESPACE_IS_NOT_EMPTY 198 /* Tablespace is not empty */
#define HA_ERR_WRONG_FILE_NAME 199         /* Invalid Filename */
#define HA_ERR_NOT_ALLOWED_COMMAND 200     /* Operation is not allowed */
#define HA_ERR_COMPUTE_FAILED 201 /* Compute generated column value failed */
/*
  Table's row format has changed in the storage engine.
  Information in the data-dictionary needs to be updated.
*/
#define HA_ERR_ROW_FORMAT_CHANGED 202
#define HA_ERR_NO_WAIT_LOCK 203     /* Don't wait for record lock */
#define HA_ERR_DISK_FULL_NOWAIT 204 /* No more room in disk */
#define HA_ERR_LAST 204             /* Copy of last error nr */

/* Number of different errors */
#define HA_ERR_ERRORS (HA_ERR_LAST - HA_ERR_FIRST + 1)

/* Other constants */

#define HA_NAMELEN 64          /* Max length of saved filename */
#define NO_SUCH_KEY (~(uint)0) /* used as a key no. */

typedef ulong key_part_map;
#define HA_WHOLE_KEY (~(key_part_map)0)

/* Intern constants in databases */

/* bits in _search */
#define SEARCH_FIND 1
#define SEARCH_NO_FIND 2
#define SEARCH_SAME 4
#define SEARCH_BIGGER 8
#define SEARCH_SMALLER 16
#define SEARCH_SAVE_BUFF 32
#define SEARCH_UPDATE 64
#define SEARCH_PREFIX 128
#define SEARCH_LAST 256
#define MBR_CONTAIN 512
#define MBR_INTERSECT 1024
#define MBR_WITHIN 2048
#define MBR_DISJOINT 4096
#define MBR_EQUAL 8192
#define MBR_DATA 16384
#define SEARCH_NULL_ARE_EQUAL 32768     /* NULL in keys are equal */
#define SEARCH_NULL_ARE_NOT_EQUAL 65536 /* NULL in keys are not equal */

/* bits in opt_flag */
#define QUICK_USED 1
#define READ_CACHE_USED 2
#define READ_CHECK_USED 4
#define KEY_READ_USED 8
#define WRITE_CACHE_USED 16
#define OPT_NO_ROWS 32

/* bits in update */
#define HA_STATE_CHANGED 1 /* Database has changed */
#define HA_STATE_AKTIV 2   /* Has a current record */
#define HA_STATE_WRITTEN 4 /* Record is written */
#define HA_STATE_DELETED 8
#define HA_STATE_NEXT_FOUND 16 /* Next found record (record before) */
#define HA_STATE_PREV_FOUND 32 /* Prev found record (record after) */
#define HA_STATE_NO_KEY 64     /* Last read didn't find record */
#define HA_STATE_KEY_CHANGED 128
#define HA_STATE_WRITE_AT_END 256 /* set in _ps_find_writepos */
#define HA_STATE_BUFF_SAVED 512   /* If current keybuff is info->buff */
#define HA_STATE_ROW_CHANGED 1024 /* To invalide ROW cache */
#define HA_STATE_EXTEND_BLOCK 2048

/* myisampack expects no more than 32 field types. */
enum en_fieldtype {
  FIELD_LAST = -1,
  FIELD_NORMAL,
  FIELD_SKIP_ENDSPACE,
  FIELD_SKIP_PRESPACE,
  FIELD_SKIP_ZERO,
  FIELD_BLOB,
  FIELD_CONSTANT,
  FIELD_INTERVALL,
  FIELD_ZERO,
  FIELD_VARCHAR,
  FIELD_CHECK,
  FIELD_enum_val_count
};

enum data_file_type {
  STATIC_RECORD,
  DYNAMIC_RECORD,
  COMPRESSED_RECORD,
  BLOCK_RECORD
};

/* For key ranges */

enum key_range_flags {
  NO_MIN_RANGE = 1 << 0,  ///< from -inf
  NO_MAX_RANGE = 1 << 1,  ///< to +inf
  /*  X < key, i.e. not including the left endpoint */
  NEAR_MIN = 1 << 2,
  /* X > key, i.e. not including the right endpoint */
  NEAR_MAX = 1 << 3,
  /*
    This flag means that index is a unique index, and the interval is
    equivalent to "AND(keypart_i = const_i)", where all of const_i are
    not NULLs.
  */
  UNIQUE_RANGE = 1 << 4,
  /*
    This flag means that the interval is equivalent to
    "AND(keypart_i = const_i)", where not all key parts may be used
    but all of const_i are not NULLs.
  */
  EQ_RANGE = 1 << 5,
  /*
    This flag has the same meaning as UNIQUE_RANGE, except that for at
    least one keypart the condition is "keypart IS NULL".
  */
  NULL_RANGE = 1 << 6,
  /**
    This flag means that the index is an rtree index, and the interval is
    specified using HA_READ_MBR_XXX defined in enum ha_rkey_function.
  */
  GEOM_FLAG = 1 << 7,
  /* Deprecated, currently used only by NDB at row retrieval */
  SKIP_RANGE = 1 << 8,
  /*
    Used to indicate that index dives can be skipped. This can happen when:
    a) Query satisfies JOIN_TAB::check_skip_records_in_range_qualification. OR
    b) Used together with EQ_RANGE to indicate that index statistics should be
       used instead of sampling the index.
  */
  SKIP_RECORDS_IN_RANGE = 1 << 9,
  /*
    Keypart is reverse-ordered (DESC) and ranges needs to be scanned
    backward. @see quick_range_seq_init, get_quick_keys.
  */
  DESC_FLAG = 1 << 10,
};

struct key_range {
  const uchar *key;
  uint length;
  key_part_map keypart_map;
  enum ha_rkey_function flag;
};

struct KEY_MULTI_RANGE {
  key_range start_key;
  key_range end_key;
  char *ptr;       /* Free to use by caller (ptr to row etc) */
  uint range_flag; /* key range flags see above */
};

/* For number of records */
#define rows2double(A) ulonglong2double(A)
typedef my_off_t ha_rows;

#define HA_POS_ERROR (~(ha_rows)0)
#define HA_OFFSET_ERROR (~(my_off_t)0)

#if SIZEOF_OFF_T == 4
#define MAX_FILE_SIZE INT_MAX32
#else
#define MAX_FILE_SIZE LLONG_MAX
#endif

#define HA_VARCHAR_PACKLENGTH(field_length) ((field_length) < 256 ? 1 : 2)

#endif /* _my_base_h */
