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

#define OTBR_LOG_TAG "MudManager"

#include "mud_manager/mud_manager.hpp"

#include <openthread/platform/toolchain.h>
#include <openthread/message.h>
#include "ncp/ncp_openthread.hpp"

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#include <list>
#include <thread>
#include <iostream>
#include <ostream>
#include <fstream>
#include <string>

#include <ctime>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "common/code_utils.hpp"
#include "utils/system_utils.hpp"

#include "../../third_party/curlcpp/repo/include/curl_easy.h"
#include "../../third_party/curlcpp/repo/include/curl_form.h"
#include "../../third_party/curlcpp/repo/include/curl_ios.h"
#include "../../third_party/curlcpp/repo/include/curl_exception.h"

#include "../../third_party/rapidjson/repo/include/rapidjson/document.h"
#include "../../third_party/rapidjson/repo/include/rapidjson/writer.h"
#include "../../third_party/rapidjson/repo/include/rapidjson/stringbuffer.h"

using namespace curl;
using namespace rapidjson;
using namespace std;


namespace otbr {
    namespace MUD {
      ostringstream mud_signature;
      
      std::string file_folder = "/home/pi/ot-br-posix/mud";
      char iptables_file[] =  "/home/pi/ot-br-posix/mud/acl.sh";

      static otMessageQueue mq;
      thread queueWatcher;

      bool closeMQ = false;
      
      MudManager::MudManager(void) {
         otbrLogInfo("Starting MUD Manager");

         otMessageQueueInit(&mq);

         otbrLogInfo("Message Queue Instanciated");
      }

      MudManager::~MudManager(void) {
         this->Deinit();
      }

      otMessageQueue* MudManager::GetMessageQueue(void) {
         return &mq;
      }

      void MudManager::Init(void) {
         
         otbrLogInfo("MUD Manager started");

         thread queueWatcher(&MudManager::CheckMessageQueue, this);
         queueWatcher.detach();
      }

      void MudManager::Deinit(void) {
         otbrLogInfo("Closing Message Queue");
         closeMQ = true;
         queueWatcher.~thread();
      }

      void MudManager::CheckMessageQueue() {

         // Define variables
         
         otMessage* msg;
         
         uint8_t  mudUrlLength;
         uint8_t  mudIpLength;
         char     mudUrl[60];
         char     mudIp[41];

         while (closeMQ == false) {
            
            if ((msg = otMessageQueueGetHead(&mq)) != nullptr) {
               otbrLogInfo("New message in queue, dequeueing...");
               otMessageQueueDequeue(&mq, msg);
               otbrLogInfo("Message dequeued");

               uint16_t num_bytes;
               if ((num_bytes = otMessageRead(msg, 0, &mudUrlLength, 8)) <= 0) {
                  otbrLogErr("Could not read message");
                  otMessageFree(msg);  

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);

                  msg = nullptr;
                  
                  continue;
               }

               otbrLogInfo("URL Length: %d", mudUrlLength);

               if ((num_bytes = otMessageRead(msg, 8, &mudUrl, mudUrlLength - 1)) <= 0) {
                  otbrLogErr("Could not read message");
                  otMessageFree(msg);  

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);

                  msg = nullptr;
                  
                  continue;
               }

               otbrLogInfo("URL: %s", mudUrl);

               if ((num_bytes = otMessageRead(msg, mudUrlLength + 8, &mudIpLength, 8)) <= 0) {
                  otbrLogErr("Could not read message");
                  otMessageFree(msg);  

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);

                  msg = nullptr;
                  
                  continue;
               }

               otbrLogInfo("Ip Length: %d", mudIpLength);

               if ((num_bytes = otMessageRead(msg, mudUrlLength + 16, &mudIp, mudIpLength)) <= 0) {
                  otbrLogErr("Could not read message");
                  otMessageFree(msg);  

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);

                  msg = nullptr;
                  
                  continue;
               }

               otbrLogInfo("Ip: %s", mudIp);

               string mudUrlString = mudUrl;
               string mudIpString = mudIp;

               otbrLogWarning("MUD Url: %s", mudUrl);
               otbrLogWarning("MUD IP: %s", mudIp);
               otbrLogWarning("MUD IP String: %s", mudIpString.c_str());

               otMessageFree(msg);
               msg = nullptr;

               otbrLogInfo("Parsing URL");
               mudUrlString = this->ParseURL(mudUrlString);
               otbrLogInfo("URL Parsed");

               ostringstream mud_content;

               if (!this->RetrieveFile(&mud_content, mudUrlString)) {
                  otbrLogErr("Error processing MUD file");

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);
               }
               
               MUDFile mf;
               
               if (!this->ParseMUDFile(&mf, &mud_content, mudIpString)) {
                  otbrLogErr("Error processing MUD file");

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);
               }

               if (!this->ImplementMUDfile(&mf)) {
                  otbrLogErr("Error processing MUD file");

                  memset(&mudUrl, 0, sizeof mudUrl);
                  memset(&mudIp, 0, sizeof mudIp);
               }
               
               otbrLogInfo("MUD File successfully converted");
   
               memset(&mudUrl, 0, sizeof mudUrl);
               memset(&mudIp, 0, sizeof mudIp);              
            }

            sleep(1);	
         }
      }

      /**
       * Create a valid MUD URL that cURL can use
       * @param url A MUD URL
       * 
       * @returns A valid MUD URL
      */
      string MudManager::ParseURL(string url)
      {
         if (url.length() == 0) {
            return "";
         }

         // Check for HTTP://, if present replace with HTTPS
         if (!strcasecmp(url.substr(0, 7).c_str(), "http://"))
         {
            url.insert(4, "s");
         }

         // If not HTTP://, check for HTTPS:// present. If not, append it
         else if (strcasecmp(url.substr(0, 8).c_str(), "https://"))
         {
            url.insert(0, "https://");
         }

         return url;
      }


      bool MudManager::RetrieveFile(ostringstream *target, string url)
      {
         // Start file download
         otbrLogInfo("Starting download of: %s", url.c_str());

         // Create buffer writer for output to variable
         curl_ios<ostringstream> writer(*target);
    
         curl_easy easy(writer);
         easy.add<CURLOPT_URL>(url.c_str());
         easy.add<CURLOPT_FOLLOWLOCATION>(1L);

         try
         {
            otbrLogInfo("Downloading file");

            easy.perform();

            otbrLogInfo("Download succeeded!");

            return true;
         }
         catch (curl_easy_exception &error)
         {
            otbrLogErr("Error: %s", error.what());

            return false;
         }
      }

      bool MudManager::ParseMUDFile(MUDFile *trgt, ostringstream* src, string ip)
      {
         Document mudFileParsed;
         // Parse the JSON into an object
         mudFileParsed.Parse(src->str().c_str());

         // Throw error if parsing did not succeed
         if (mudFileParsed.HasParseError()) {
            return false;
         }

         // Convert JSON Object to internal object
         Value& mud = mudFileParsed["ietf-mud:mud"];

         trgt->device_ip = ip.c_str();
         trgt->mud_version = mud["mud-version"].GetInt();
         trgt->mud_url = mud["mud-url"].GetString();
         trgt->last_update = mud["last-update"].GetString();
         trgt->mud_signature = mud["mud-signature"].GetString();
         trgt->cache_validity = mud["cache-validity"].GetInt();
         trgt->is_supported = mud["is-supported"].GetBool();
         trgt->systeminfo = mud["systeminfo"].GetString();
         trgt->mfg_name = mud["mfg-name"].GetString();
         trgt->model_name = mud["model-name"].GetString();
         trgt->firmware_rev = mud["firmware-rev"].GetString();
         trgt->documentation = mud["documentation"].GetString();
         trgt->extensions = mud["extensions"].GetString();

         Value& from = mud["from-device-policy"]["access-lists"]["access-list"];
         for (auto& f : from.GetArray()) {
            trgt->from_device_policies.push_back(f["name"].GetString());
         } 

         Value& to = mud["to-device-policy"]["access-lists"]["access-list"];
         for (auto& t : to.GetArray()) {
            trgt->to_device_policies.push_back(t["name"].GetString());
         } 

         Value& acls = mudFileParsed["ietf-access-control-list:acls"]["acl"];

         for (auto& acl : acls.GetArray()) {
            ACL mACL = ACL();
            mACL.name = acl["name"].GetString();
            otbrLogInfo("Processing ACL: %s", mACL.name);
            mACL.type = acl["type"].GetString();

            Value& aces = acl["aces"]["ace"];

            for (auto& ace : aces.GetArray()) {
               ACE mACE = ACE();

               mACE.name = ace["name"].GetString();
               mACE.forwarding = ace["actions"]["forwarding"].GetString();

               otbrLogInfo("Processing ACE: %s", mACE.name);

               Match mMatch = Match();

               Value& matches = ace["matches"];

               if (matches.HasMember("ietf-mud:mud")) {
                  Value& mud = matches["ietf-mud:mud"];

                  if (mud.HasMember("controller")) {
                     mMatch.controller = mud["controller"].GetString();
                  }
               }

               if (matches.HasMember("ipv6")) {
                  Value& ipv6 = matches["ipv6"];
                  if (ipv6.HasMember("ietf-acldns:src-dnsname")) {
                     mMatch.src_dnsname = ipv6["ietf-acldns:src-dnsname"].GetString();
                  }  
                  if (ipv6.HasMember("ietf-acldns:dst-dnsname")) {
                     mMatch.dst_dnsname = ipv6["ietf-acldns:dst-dnsname"].GetString();
                  }

                  mMatch.protocol = ipv6["protocol"].GetInt();
               }

               if (matches.HasMember("ipv4")) {
                  Value& ipv4 = matches["ipv4"];
                  if (ipv4.HasMember("ietf-acldns:src-dnsname")) {
                     mMatch.src_dnsname = ipv4["ietf-acldns:src-dnsname"].GetString();
                  }
                  if (ipv4.HasMember("ietf-acldns:dst-dnsname")) {
                     mMatch.dst_dnsname = ipv4["ietf-acldns:dst-dnsname"].GetString();
                  }

                  mMatch.protocol = ipv4["protocol"].GetInt();
               }

               if (matches.HasMember("tcp")) {
                  Value& tcp = matches["tcp"];

                  if (tcp.HasMember("ietf-mud:direction-initiated")) {
                     mMatch.direction_initiated = tcp["ietf-mud:direction-initiated"].GetString();
                  }

                  if (tcp.HasMember("source-port")) {
                     Value& src = tcp["source-port"];

                     if (src.HasMember("operator")) {
                        mMatch.src_op = src["operator"].GetString();
                     }

                     if (src.HasMember("port")) {
                        mMatch.src_port = src["port"].GetInt();
                     }
                     
                  }

                  if (tcp.HasMember("destination-port")) {
                     Value& dst = tcp["destination-port"];

                     if (dst.HasMember("operator")) {
                        mMatch.dst_op = dst["operator"].GetString();
                     }

                     if (dst.HasMember("port")) {
                        mMatch.dst_port = dst["port"].GetInt();
                     }
                     
                  }
               }

               if (matches.HasMember("udp")) {
                  Value& udp = matches["udp"];

                  if (udp.HasMember("ietf-mud:direction-initiated")) {
                     mMatch.direction_initiated = udp["ietf-mud:direction-initiated"].GetString();
                  }

                  if (udp.HasMember("source-port")) {
                     Value& src = udp["source-port"];

                     if (src.HasMember("operator")) {
                        mMatch.src_op = src["operator"].GetString();
                     }

                     if (src.HasMember("port")) {
                        mMatch.src_port = src["port"].GetInt();
                     }
                     
                  }

                  if (udp.HasMember("destination-port")) {
                     Value& dst = udp["destination-port"];

                     if (dst.HasMember("operator")) {
                        mMatch.dst_op = dst["operator"].GetString();
                     }

                     if (dst.HasMember("port")) {
                        mMatch.dst_port = dst["port"].GetInt();
                     }
                     
                  }
               } 

               if (matches.HasMember("icmpv6")) {
                  Value& icmp = matches["icmpv6"];

                  if (icmp.HasMember("ietf-mud:direction-initiated")) {
                     mMatch.direction_initiated = icmp["ietf-mud:direction-initiated"].GetString();
                  }

                  if (icmp.HasMember("source-port")) {
                     Value& src = icmp["source-port"];

                     if (src.HasMember("operator")) {
                        mMatch.src_op = src["operator"].GetString();
                     }

                     if (src.HasMember("port")) {
                        mMatch.src_port = src["port"].GetInt();
                     }
                     
                  }

                  if (icmp.HasMember("destination-port")) {
                     Value& dst = icmp["destination-port"];

                     if (dst.HasMember("operator")) {
                        mMatch.dst_op = dst["operator"].GetString();
                     }

                     if (dst.HasMember("port")) {
                        mMatch.dst_port = dst["port"].GetInt();
                     }
                     
                  }
               } 

               mACE.matches = mMatch;
               mACL.aces.push_back(mACE);
  
            }

            otbrLogInfo("ACL: %s | ACE Count: %d", mACL.name, mACL.aces.size());

            for (const char* a : trgt->from_device_policies) {
               if ( strcmp(a, mACL.name) == 0 ) {
                  otbrLogInfo("Adding from device policy: %s", mACL.name);
                  trgt->from_device_acls.push_back(mACL);
               }
            }

            for (const char* a : trgt->to_device_policies) {
               if ( strcmp(a, mACL.name) == 0 ) {
                  otbrLogInfo("Adding to device policy: %s", mACL.name);
                  trgt->to_device_acls.push_back(mACL);
               }
            }

            
         }

         otbrLogInfo("Converted MUD File to MUD Struct.");

         otbrLogInfo("Incoming ACLs: %d", trgt->to_device_acls.size());
         otbrLogInfo("Incoming Policies: %d", trgt->to_device_policies.size());

         otbrLogInfo("Outgoing ACLs: %d", trgt->from_device_acls.size());
         otbrLogInfo("Outgoing Policies: %d", trgt->from_device_policies.size());

         otbrLogInfo("Finished creating MUD Structure");

         return true;
      }

      bool MudManager::ImplementMUDfile(MUDFile *mf) {
         // Check if folder exists for iptables
         struct stat statbuf; 
         int isDir = 0; 

         if (stat(file_folder.c_str(), &statbuf) == 0) { 
            if (S_ISDIR(statbuf.st_mode)) { 
               isDir = 1; 
            } 
         }

         if (isDir == 0) {
            otbrLogInfo("ACL folder does not exist");

            if (!mkdir(file_folder.c_str(), 0777))
               otbrLogInfo("Directory created");
            else {
               otbrLogInfo("Unable to create directory");
               return false;
            }    
         }

         otbrLogInfo("Folder %s exists!", file_folder.c_str());

         otbrLogInfo("Creating ip6tables file");

         string device_ip = mf->device_ip;
         string device_ip_stripped = device_ip;
         device_ip_stripped.erase(std::remove(device_ip_stripped.begin(), device_ip_stripped.end(), ':'), device_ip_stripped.end());
         
         string policy = this->RandomPolicy(20);
        
         string policy_name_in = policy + "_in";
         string policy_name_out = policy + "_out";
         string file_name = device_ip_stripped + ".sh";
         string file_path = file_folder + "/" + file_name;

         otbrLogWarning("File path: %s", file_path.c_str());

         // struct stat fileBuffer;   
         // if (stat(file_path.c_str(), &fileBuffer) == 0) {
         //    SystemUtils::ExecuteCommand("bash %s down", file_path.c_str());
         // }

         std::ofstream outfile (file_path);

         outfile << "#!/bin/bash" << endl;
         outfile << endl;
         outfile << "IP=" << device_ip << endl;
         outfile << "POLICY_IN=" << policy_name_in << endl;
         outfile << "POLICY_OUT=" << policy_name_out << endl;
         outfile << endl;
         outfile << "if [[ $1 == \"down\" ]]; then" << endl;
         outfile << endl;
         outfile << "ip6tables -D FORWARD -d $IP -j $POLICY_IN" << endl;
         outfile << "ip6tables -F $POLICY_IN" << endl;
         outfile << "ip6tables -X $POLICY_IN" << endl;
         outfile << endl;
         outfile << "ip6tables -D FORWARD -s $IP -j $POLICY_OUT" << endl;
         outfile << "ip6tables -F $POLICY_OUT" << endl;
         outfile << "ip6tables -X $POLICY_OUT" << endl;
         outfile << endl;
         outfile << "fi" << endl;
         outfile << endl;
         outfile << "if [[ $1 == \"up\" ]]; then" << endl;
         outfile << endl;
         outfile << "ip6tables -N $POLICY_IN" << endl;
         outfile << "ip6tables -N $POLICY_OUT" << endl;

         for (ACL a : mf->from_device_acls) {
            outfile << endl;
            outfile << "# ACL: " << a.name << " | Type: " << a.type << endl;
            for (ACE e : a.aces) {
               outfile << "## ACE: " << e.name << endl;
               ostringstream line;
               line << "ip6tables -A $POLICY_OUT";

               if (e.matches.protocol == 1) {
                  line << " -p tcp";
               } else if (e.matches.protocol == 6) {
                  line << " -p tcp";
               } else if (e.matches.protocol == 17) {
                  line << " -p udp";
               } else if (e.matches.protocol == 58) {
                  line << " -p icmpv6";
               }

               if ((e.matches.src_dnsname != NULL) && (strlen(e.matches.src_dnsname) > 0)) {
                  line << " -s " << e.matches.src_dnsname;
               }

               if ((e.matches.dst_dnsname != NULL) && (strlen(e.matches.dst_dnsname) > 0)) {
                  line << " -d " << e.matches.dst_dnsname;
               }

               if (e.matches.dst_port > 0) {
                  line << " --dport " << e.matches.dst_port;
               }

               if (e.matches.src_port > 0) {
                  line << " --sport " << e.matches.src_port;
               }

               line << " -j ACCEPT";

               outfile << line.str() << endl;
            }
         }

         otbrLogInfo("Finished Creating From Device ACLs");

         otbrLogInfo("To Device ACLs: %d", mf->to_device_acls.size());

         for (ACL a : mf->to_device_acls) {
            outfile << endl;
            outfile << "# ACL: " << a.name << " | Type: " << a.type << endl;
            otbrLogInfo("To Device ACEs: %d", a.aces.size());
            for (ACE e : a.aces) {
               outfile << "## ACE: " << e.name << endl;
               ostringstream line;
               line << "ip6tables -A $POLICY_IN";
               
               if (e.matches.protocol == 1) {
                  line << " -p tcp";
               } else if (e.matches.protocol == 6) {
                  line << " -p tcp";
               } else if (e.matches.protocol == 17) {
                  line << " -p udp";
               } else if (e.matches.protocol == 58) {
                  line << " -p icmpv6";
               }

               if ((e.matches.src_dnsname != NULL) && (strlen(e.matches.src_dnsname) > 0)) {
                  line << " -s " << e.matches.src_dnsname;
               }

               if ((e.matches.dst_dnsname != NULL) && (strlen(e.matches.dst_dnsname) > 0)) {
                  line << " -d " << e.matches.dst_dnsname;
               }

               if (e.matches.dst_port > 0) {
                  line << " --dport " << e.matches.dst_port;
               }

               if (e.matches.src_port > 0) {
                  line << " --sport " << e.matches.src_port;
               }

               line << " -j ACCEPT";
               outfile << line.str() << endl;
            }
         }
         outfile << endl;
         outfile << "ip6tables -A $POLICY_IN -j LOG --log-prefix \"MUD-Dropped: \" --log-level 4" << endl;
         outfile << "ip6tables -A $POLICY_OUT -j LOG --log-prefix \"MUD-Dropped: \" --log-level 4" << endl;
         outfile << endl;
         outfile << "ip6tables -A $POLICY_IN -j DROP" << endl;
         outfile << "ip6tables -A $POLICY_OUT -j DROP" << endl;
         outfile << endl;
         outfile << "ip6tables -I FORWARD 1 -d $IP -j $POLICY_IN" << endl;
         outfile << "ip6tables -I FORWARD 1 -s $IP -j $POLICY_OUT" << endl;
         outfile << endl;
         outfile << "fi" << endl;

         otbrLogInfo("Finished creating IpTables");

         outfile.close();

         otbrLogInfo("File Closed");

         SystemUtils::ExecuteCommand("chmod +x %s", file_path.c_str());
         // SystemUtils::ExecuteCommand("bash %s up", file_path.c_str());

         otbrLogInfo("Executed file");

         return true;
      }

      string MudManager::RandomPolicy(int length) {
         static const char AlphaNumeric[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz";
         string r = "";

         for (int i = 0 ; i < length ; i++) {
            r += AlphaNumeric[rand() % (sizeof(AlphaNumeric) - 1) ];
         }

         return r;
      }
    }
 }
