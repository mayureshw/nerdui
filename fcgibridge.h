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
            auto raw_query = FCGX_GetParam("QUERY_STRING", req.envp);
            auto query = Query::parse(raw_query);
            auto response = getResponse(query);
            FCGX_PutS("Content-Type: text/html\r\n\r\n", req.out);
            FCGX_PutStr(response.c_str(), response.size(), req.out);
            FCGX_Finish_r(&req);
        }
    }
};

#endif
