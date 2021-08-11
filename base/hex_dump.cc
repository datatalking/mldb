/* hex_dump.cc                                                      -*- C++ -*-
   Jeremy Barnes, 6 October 2010
   Copyright (c) 2010 Jeremy Barnes.  All rights reserved.
   Copyright (c) 2010 mldb.ai inc.  All rights reserved.
   This file is part of MLDB. Copyright 2015 mldb.ai inc. All rights reserved.

   Routine to dump memory in hex format.
*/

#include "mldb/base/hex_dump.h"
#include <iostream>
#include "mldb/utils/string_functions.h"

using namespace std;

namespace MLDB {

void hex_dump(const void * mem, size_t total_memory, size_t max_size, std::ostream & stream)
{
    const char * buffer = (const char *)mem;

    for (unsigned i = 0;  i < total_memory && i < max_size;  i += 16) {
        stream << MLDB::format("%04x | ", i);
        for (unsigned j = i;  j < i + 16;  ++j) {
            if (j < total_memory)
                cerr << MLDB::format("%02x ", (int)*(unsigned char *)(buffer + j));
            else cerr << "   ";
        }
        
        stream << "| ";
        
        for (unsigned j = i;  j < i + 16;  ++j) {
            if (j < total_memory) {
                if (buffer[j] >= ' ' && buffer[j] < 127)
                    cerr << buffer[j];
                else cerr << '.';
            }
            else cerr << " ";
        }
        stream << endl;
    }
}

  void hex_dump(std::string_view mem, size_t max_size, std::ostream & stream)
{
  hex_dump(mem.data(), mem.length(), max_size, stream);
}

} // namespace MLDB
