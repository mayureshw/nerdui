#ifndef _FCGIBRIDGE_H
#define _FCGIBRIDGE_H

#include <fcgiapp.h>
#include <salzaverde/query.h>
#include <string>

using namespace std;
using namespace salzaverde;

class FCGIBridge {
public:
    virtual string getResponse(Query& query)=0;
    void loop()
    {
        FCGX_Request req;
        FCGX_Init();
        FCGX_InitRequest(&req, 0, 0);
        while (FCGX_Accept_r(&req) == 0)
        {
            const char* clen_str = FCGX_GetParam("CONTENT_LENGTH", req.envp);
            int clen = clen_str ? stoi(clen_str) : 0;
            string post_data(clen, '\0');
            FCGX_GetStr(post_data.data(), clen, req.in); // read POST body
            auto post_query = Query::parse(post_data);   // parse URL-encoded

            auto get_query_raw = FCGX_GetParam("QUERY_STRING", req.envp);
            auto get_query = Query::parse(get_query_raw);
            for(auto k : get_query.list()) post_query[k] = get_query[k];

            auto response = getResponse(post_query);
            FCGX_PutS("Content-Type: text/html\r\n\r\n", req.out);
            FCGX_PutStr(response.c_str(), response.size(), req.out);
            FCGX_Finish_r(&req);
        }
    }
};

#endif
