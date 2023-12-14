#include <stdio.h>
#include "httpserver.hpp"
#include <string>
#include <windows.h>
#include <iostream>
#include <fstream>
#include "service_controller.h"
#include "kuzu.h"
#include <sstream>

using namespace std;
using namespace httpserver;
//.
namespace
{
    SERVICE_STATUS g_ServiceStatus = {0};
    SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
}

//-----------------------------------------------------------------------------------------------

class DataResource : public httpserver::http_resource
{
    kuzu_database *db;
    kuzu_connection *conn;

public:
    DataResource()
    {
        db = kuzu_database_init("C:\\kuzu-test", kuzu_default_system_config());
        conn = kuzu_connection_init(db);
    }

    std::pair<std::string, int64_t> parseNameAndAgeFromContent(const std::string &content);

    std::shared_ptr<httpserver::http_response> render(const httpserver::http_request &);

    std::shared_ptr<httpserver::http_response> getData(const httpserver::http_request &);
    std::shared_ptr<httpserver::http_response> putData(const httpserver::http_request &);
    std::shared_ptr<httpserver::http_response> postData(const httpserver::http_request &);
    std::shared_ptr<httpserver::http_response> deleteData(const httpserver::http_request &);
};

std::shared_ptr<httpserver::http_response> DataResource::render(const httpserver::http_request &req)
{
    if (req.get_method() == "GET" && req.get_path() == "/api/getData")
    {
        return getData(req);
    }
    if (req.get_method() == "PUT" && req.get_path() == "/api/putData")
    {
        return putData(req);
    }

    if (req.get_method() == "POST" && req.get_path() == "/api/postData")
    {
        return postData(req);
    }

    if (req.get_method() == "DELETE" && req.get_path() == "/api/deleteData")
    {
        return deleteData(req);
    }

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Invalid endpoint", 400));
}

std::shared_ptr<httpserver::http_response> DataResource::getData(const httpserver::http_request &req)
{
    kuzu_query_result *result = kuzu_connection_query(conn, "MATCH (a:Person) RETURN a.name AS NAME, a.age AS AGE;");

    std::string responseContent;
    while (kuzu_query_result_has_next(result))
    {
        kuzu_flat_tuple *tuple = kuzu_query_result_get_next(result);
        kuzu_value *value = kuzu_flat_tuple_get_value(tuple, 0);
        std::string name = kuzu_value_get_string(value);
        value = kuzu_flat_tuple_get_value(tuple, 1);
        int64_t age = kuzu_value_get_int64(value);
        kuzu_value_destroy(value);
        responseContent += "Name: " + name + ", Age: " + std::to_string(age) + "\n";
        kuzu_flat_tuple_destroy(tuple);
    }

    kuzu_query_result_destroy(result);

    return std::make_shared<string_response>(responseContent, 200);
    // ...
}

std::shared_ptr<httpserver::http_response> DataResource::putData(const httpserver::http_request &req)
{
      ofstream logFile("C:\\logger.txt", ios::app);

    std::string content(req.get_content().begin(), req.get_content().end());
    logFile << content << endl;

    try
    {
        auto [name, age] = parseNameAndAgeFromContent(content);
        std::string checkQuery = "MATCH (p:Person {name: '" + name + "'}) RETURN p;";
        kuzu_query_result *checkResult = kuzu_connection_query(conn, checkQuery.c_str());

        if (!kuzu_query_result_has_next(checkResult))
        {
            // If the Person node doesn't exist, create it
            std::string createQuery = "CREATE (:Person {name: '" + name + "', age: " + std::to_string(age) + "});";
            kuzu_query_result *createResult = kuzu_connection_query(conn, createQuery.c_str());
            kuzu_query_result_destroy(createResult);

            return std::make_shared<httpserver::string_response>("Person created successfully", 200);
        }

        kuzu_query_result_destroy(checkResult);

        std::string updateQuery = "MATCH (p:Person {name: '" + name + "'}) SET p.age = " + std::to_string(age) + ";";
        kuzu_query_result *updateResult = kuzu_connection_query(conn, updateQuery.c_str());
        kuzu_query_result_destroy(updateResult);

        return std::make_shared<httpserver::string_response>("Person age updated successfully", 200);

    }
    catch (const std::invalid_argument &e)
    {
        return std::make_shared<httpserver::string_response>(e.what(), 400);
    }

}

std::shared_ptr<httpserver::http_response> DataResource::postData(const httpserver::http_request &req)
{
    
        std::string content(req.get_content().begin(), req.get_content().end());

    try {
        auto [name, age] = parseNameAndAgeFromContent(content);

        // Check if the Person node with the specified name exists in the Kuzu graph database
        std::string checkQuery = "MATCH (p:Person {name: '" + name + "'}) RETURN p;";
        kuzu_query_result* checkResult = kuzu_connection_query(conn, checkQuery.c_str());

        if (!kuzu_query_result_has_next(checkResult)) {
            // If the Person node doesn't exist, create it
            std::string createQuery = "CREATE (:Person {name: '" + name + "', age: " + std::to_string(age) + "});";
            kuzu_query_result* createResult = kuzu_connection_query(conn, createQuery.c_str());
            kuzu_query_result_destroy(createResult);

            return std::make_shared<httpserver::string_response>("Person created successfully", 200);
        }

        kuzu_query_result_destroy(checkResult);

        return std::make_shared<httpserver::string_response>("Person with the specified name already exists", 400);
    } catch (const std::invalid_argument& e) {
        return std::make_shared<httpserver::string_response>(e.what(), 400);
    }
    // ...
}

std::shared_ptr<httpserver::http_response> DataResource::deleteData(const httpserver::http_request &req)
{
        std::string content(req.get_content().begin(), req.get_content().end());

    try {
        auto [name, age] = parseNameAndAgeFromContent(content);

        // Check if the Person node with the specified name exists in the Kuzu graph database
        std::string checkQuery = "MATCH (p:Person {name: '" + name + "'}) RETURN p;";
        kuzu_query_result* checkResult = kuzu_connection_query(conn, checkQuery.c_str());

        if (kuzu_query_result_has_next(checkResult)) {
            // If the Person node exists, delete it
            std::string deleteQuery = "MATCH (p:Person {name: '" + name + "'}) DELETE p;";
            kuzu_query_result* deleteResult = kuzu_connection_query(conn, deleteQuery.c_str());
            kuzu_query_result_destroy(deleteResult);

            return std::make_shared<httpserver::string_response>("Person deleted successfully", 200);
        }

        kuzu_query_result_destroy(checkResult);

        return std::make_shared<httpserver::string_response>("Person with the specified name not found", 404);
    } catch (const std::invalid_argument& e) {
        return std::make_shared<httpserver::string_response>(e.what(), 400);
    }
    // ...
}

std::pair<std::string, int64_t> DataResource::parseNameAndAgeFromContent(const std::string &content)
{
    size_t pos = content.find("\"name\":");
    if (pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid request format: 'name' not found");
    }
    pos = content.find("\"", pos + 7);
    if (pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid request format: Invalid 'name' format");
    }
    size_t endPos = content.find("\"", pos + 1);
    if (endPos == std::string::npos)
    {
        throw std::invalid_argument("Invalid request format: Unterminated 'name'");
    }
    std::string name = content.substr(pos + 1, endPos - pos - 1);

    pos = content.find("\"age\":");
    if (pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid request format: 'age' not found");
    }
    pos = content.find(":", pos + 5);
    if (pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid request format: Invalid 'age' format");
    }
    endPos = content.find("}", pos + 1);
    if (endPos == std::string::npos)
    {
        throw std::invalid_argument("Invalid request format: Unterminated 'age'");
    }
    std::string ageStr = content.substr(pos + 1, endPos - pos - 1);

    // Convert age to int64_t
    int64_t age;
    try
    {
        age = std::stoll(ageStr);
    }
    catch (const std::exception &e)
    {
        throw std::invalid_argument("Invalid request format: Invalid 'age' format");
    }

    return std::make_pair(name, age);
}


//--------------------------------------------------------------

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

void ServiceMain(int argc, char **argv);
void ControlHandler(DWORD request);
void ServiceWorkerThread();
void ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
void kuzu();

int main(int argc, char *argv[])
{
    SERVICE_TABLE_ENTRY ServiceTable[2];
    ServiceTable[0].lpServiceName = const_cast<LPSTR>("KuzuService");
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;
    StartServiceCtrlDispatcher(ServiceTable);

    if (argc <= 1)
    {
        return 0;
    }
    string command = argv[1];
    std::wstring serviceName = L"KuzuService";
    controller::winService ControlHandler = controller::winService(serviceName, serviceName);
    if (command == "start")
    {
        ControlHandler.start();
    }
    else if (command == "install")
    {
        ControlHandler.install(argv);
    }
    else if (command == "uninstall")
    {
        ControlHandler.uninstall();
    }
    else if (command == "stop")
    {
        ControlHandler.stop();
    }
}

void ServiceMain(int argc, char **argv)
{
    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint =  0;

    hStatus = RegisterServiceCtrlHandlerA("KuzuService", (LPHANDLER_FUNCTION)ControlHandler);

    if (hStatus == (SERVICE_STATUS_HANDLE)0)
    {
        return;
    }

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    if (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        ServiceWorkerThread();
    }
}

void ControlHandler(DWORD request)
{
    if (request == SERVICE_CONTROL_STOP || request == SERVICE_CONTROL_SHUTDOWN)
    {
        ServiceStatus.dwWin32ExitCode = 0;
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;
    }
}

void ServiceWorkerThread()
{

    httpserver::webserver ws = httpserver::create_webserver(8080).start_method(httpserver::http::http_utils::INTERNAL_SELECT).max_threads(5);

    DataResource dataResource;

    // Register the NotesResource with the server
    ws.register_resource("/api", &dataResource, true);

    ws.start(true);
    ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

void ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    // Update the service status
    g_ServiceStatus.dwCurrentState = dwCurrentState;
    g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_ServiceStatus.dwWaitHint = dwWaitHint;

    // Report the status to the SCM.
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

