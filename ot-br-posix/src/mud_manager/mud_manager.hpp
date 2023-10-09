/*
 *    Copyright (c) 2023, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes implementation of the MUD Manager.
 */

#ifndef OTBR_MUD_MANAGER_HPP_
#define OTBR_MUD_MANAGER_HPP_

#include <string>
#include <list>
#include <iostream>
#include <ostream>
#include <fstream>

#include "utils/system_utils.hpp"

#include <openthread/message.h>

using namespace std;

// #include "../../third_party/cpp-httplib/repo/httplib.h"

enum ACE_TYPE { ipv4, ipv6 };

struct Match {
   ACE_TYPE type; // ipv4 || ipv6
   const char* src_dnsname;
   const char* dst_dnsname;
   uint8_t protocol;
   const char* direction_initiated;
   const char* src_op;
   uint16_t src_port;
   const char* dst_op;
   uint16_t dst_port;
   const char* controller;
};

struct ACE {
   const char* name;
   const char* forwarding;
   Match matches;
};

struct ACL {
   const char* name;
   const char* type;
   list <ACE> aces;
};

struct MUDFile {
   uint8_t mud_version;
   const char* mud_url;
   const char* last_update;
   const char* mud_signature;
   uint8_t cache_validity;
   bool is_supported;
   const char* systeminfo;
   const char* mfg_name;
   const char* model_name;
   const char* firmware_rev;
   const char* software_rev;
   const char* documentation;
   const char* extensions;
   string device_ip;
   list<const char *> from_device_policies;
   list<const char *> to_device_policies;
   list <ACL> from_device_acls;
   list <ACL> to_device_acls;
};

namespace otbr {
    namespace MUD {
        class MudManager {
            public:

                /**
                 * This constructor creates a MUD Manager Object.
                 *
                 */
                explicit MudManager(void);

                /**
                 * This destructor destroys a MUD Manager Object.
                 *
                 */
                ~MudManager(void);

                /**
                 * This method initializes a ND Proxy manager instance.
                 *
                 */
                void Init(void);

                void Deinit(void);

                otMessageQueue* GetMessageQueue(void);

                void CheckMessageQueue();

                /**
                 * Start Processing of the actual MUD URL
                 *
                 */
                void Process(void);

                /**
                * Create a valid MUD URL that cURL can use
                * @param url A MUD URL
                * 
                * @returns A valid MUD URL
                */
                string ParseURL(string url);

                /**
                 * Retrieve a MUD File from a server
                 * 
                 * @returns A pointer to the MUD File Content
                 * 
                */
                bool RetrieveFile(ostringstream *target, string url);

                // /**
                //  * Convert the MUD content into a C object
                // */
                bool ParseMUDFile(MUDFile *trgt, ostringstream* src, string ip);

                // /**
                //  * Create a bash script that can insert the MUD rules into ip6tables
                // */
                bool ImplementMUDfile(MUDFile *mf);

                string RandomPolicy(int length);
        };
    }
}

#endif