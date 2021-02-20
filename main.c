/*
 * Copyright (c) 2018 the CivetWeb developers
 * MIT License
 */

/* Simple demo of a REST callback. */
#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS
#else
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "civetweb.h"

#define PORT "8089"
#define HOST_INFO "http://localhost:8089"

#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"

int exitNow = 0;

static unsigned request = 0; /* demo data: request counter */

int ExitHandler(struct mg_connection *conn, void *cbdata)
{
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: "
            "text/plain\r\nConnection: close\r\n\r\n");
  mg_printf(conn, "Server will shut down.\n");
  mg_printf(conn, "Bye!\n");
  exitNow = 1;
  return 1;
}

int log_message(const struct mg_connection *conn, const char *message)
{
  puts(message);
  return 1;
}

FileHandler(struct mg_connection *conn, void *cbdata)
{
  const struct mg_request_info *ri = mg_get_request_info(conn);
  printf("%s\n", ri->request_uri);
  printf("%s\n", ri->local_uri);

  /* In this handler, we ignore the req_info and send the file "fileName". */
  const char *fileName = (const char *)cbdata;

  mg_send_file(conn, fileName);
  return 1;
}

AssetHandler(struct mg_connection *conn, void *cbdata)
{
  const struct mg_request_info *ri = mg_get_request_info(conn);

  const char *folderName = (const char *)cbdata;
  char *result = malloc(strlen(folderName) + strlen(ri->local_uri) + 1); // +1 for the null-terminator
  // in real code you would check for errors in malloc here
  strcpy(result, folderName);
  strcat(result, ri->local_uri);
  printf("%s\n", result);
  mg_send_file(conn, result);
  free(result);

  /* In this handler, we ignore the req_info and send the file "fileName". */

  return 1;
}

#define USE_WEBSOCKET
#ifdef USE_WEBSOCKET

/* MAX_WS_CLIENTS defines how many clients can connect to a websocket at the
 * same time. The value 5 is very small and used here only for demonstration;
 * it can be easily tested to connect more than MAX_WS_CLIENTS clients.
 * A real server should use a much higher number, or better use a dynamic list
 * of currently connected websocket clients. */
#define MAX_WS_CLIENTS (5)

struct t_ws_client
{
  /* Handle to the connection, used for mg_read/mg_write */
  struct mg_connection *conn;

  /*
	    WebSocketConnectHandler sets state to 1 ("connected")
	    the connect handler can accept or reject a connection, but it cannot
	    send or receive any data at this state

	    WebSocketReadyHandler sets state to 2 ("ready")
	    reading and writing is possible now

	    WebSocketCloseHandler sets state to 0
	    the websocket is about to be closed, reading and writing is no longer
	    possible this callback can be used to cleanup allocated resources

	    InformWebsockets is called cyclic every second, and sends some data
	    (a counter value) to all websockets in state 2
	*/
  int state;
} static ws_clients[MAX_WS_CLIENTS];

#define ASSERT(x)                              \
  {                                            \
    if (!(x))                                  \
    {                                          \
      fprintf(stderr,                          \
              "Assertion failed in line %u\n", \
              (unsigned)__LINE__);             \
    }                                          \
  }

int WebSocketConnectHandler(const struct mg_connection *conn, void *cbdata)
{
  struct mg_context *ctx = mg_get_context(conn);
  int reject = 1;
  int i;

  mg_lock_context(ctx);
  for (i = 0; i < MAX_WS_CLIENTS; i++)
  {
    if (ws_clients[i].conn == NULL)
    {
      ws_clients[i].conn = (struct mg_connection *)conn;
      ws_clients[i].state = 1;
      mg_set_user_connection_data(ws_clients[i].conn,
                                  (void *)(ws_clients + i));
      reject = 0;
      break;
    }
  }
  mg_unlock_context(ctx);

  fprintf(stdout,
          "Websocket client %s\r\n\r\n",
          (reject ? "rejected" : "accepted"));
  return reject;
}

void WebSocketReadyHandler(struct mg_connection *conn, void *cbdata)
{
  const char *text = "Hello from the websocket ready handler";
  struct t_ws_client *client = mg_get_user_connection_data(conn);

  mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
  fprintf(stdout, "Greeting message sent to websocket client\r\n\r\n");
  ASSERT(client->conn == conn);
  ASSERT(client->state == 1);

  client->state = 2;
}

int WebsocketDataHandler(struct mg_connection *conn,
                         int bits,
                         char *data,
                         size_t len,
                         void *cbdata)
{
  struct t_ws_client *client = mg_get_user_connection_data(conn);
  ASSERT(client->conn == conn);
  ASSERT(client->state >= 1);

  fprintf(stdout, "Websocket got %lu bytes of ", (unsigned long)len);
  switch (((unsigned char)bits) & 0x0F)
  {
  case MG_WEBSOCKET_OPCODE_CONTINUATION:
    fprintf(stdout, "continuation");
    break;
  case MG_WEBSOCKET_OPCODE_TEXT:
    fprintf(stdout, "text");
    break;
  case MG_WEBSOCKET_OPCODE_BINARY:
    fprintf(stdout, "binary");
    break;
  case MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE:
    fprintf(stdout, "close");
    break;
  case MG_WEBSOCKET_OPCODE_PING:
    fprintf(stdout, "ping");
    break;
  case MG_WEBSOCKET_OPCODE_PONG:
    fprintf(stdout, "pong");
    break;
  default:
    fprintf(stdout, "unknown(%1xh)", ((unsigned char)bits) & 0x0F);
    break;
  }
  fprintf(stdout, " data:\r\n");
  fwrite(data, len, 1, stdout);
  fprintf(stdout, "\r\n\r\n");

  return 1;
}

void WebSocketCloseHandler(const struct mg_connection *conn, void *cbdata)
{
  struct mg_context *ctx = mg_get_context(conn);
  struct t_ws_client *client = mg_get_user_connection_data(conn);
  ASSERT(client->conn == conn);
  ASSERT(client->state >= 1);

  mg_lock_context(ctx);
  while (client->state == 3)
  {
    /* "inform" state, wait a while */
    mg_unlock_context(ctx);
#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
    mg_lock_context(ctx);
  }
  client->state = 0;
  client->conn = NULL;
  mg_unlock_context(ctx);

  fprintf(stdout,
          "Client dropped from the set of webserver connections\r\n\r\n");
}

void InformWebsockets(struct mg_context *ctx)
{
  static unsigned long cnt = 0;
  char text[32];
  size_t textlen;
  int i;

  sprintf(text, "%lu", ++cnt);
  textlen = strlen(text);

  for (i = 0; i < MAX_WS_CLIENTS; i++)
  {
    int inform = 0;

    mg_lock_context(ctx);
    if (ws_clients[i].state == 2)
    {
      /* move to "inform" state */
      ws_clients[i].state = 3;
      inform = 1;
    }
    mg_unlock_context(ctx);

    if (inform)
    {
      mg_websocket_write(ws_clients[i].conn,
                         MG_WEBSOCKET_OPCODE_TEXT,
                         text,
                         textlen);
      mg_lock_context(ctx);
      ws_clients[i].state = 2;
      mg_unlock_context(ctx);
    }
  }
}
#endif

int main(int argc, char *argv[])
{
  const char *options[] = {"listening_ports",
                           PORT,
                           "request_timeout_ms",
                           "10000",
                           "error_log_file",
                           "error.log",
                           "enable_auth_domain_check",
                           "no",
#ifdef USE_WEBSOCKET
                           "websocket_timeout_ms",
                           "3600000",
#endif
                           0};

  struct mg_callbacks callbacks;
  struct mg_context *ctx;
  int err = 0;

  mg_init_library(0);
  if (err)
  {
    fprintf(stderr, "Cannot start CivetWeb - inconsistent build.\n");
    return EXIT_FAILURE;
  }

  if (!mg_check_feature(16))
  {
    fprintf(stderr,
            "Error: Embedded example built with websocket support, "
            "but civetweb library build without.\n");
    err = 1;
  }
  if (err)
  {
    fprintf(stderr, "Cannot start CivetWeb - inconsistent build.\n");
    return EXIT_FAILURE;
  }

  /* Callback will print error messages to console */
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.log_message = log_message;

  /* Start CivetWeb web server */
  ctx = mg_start(&callbacks, 0, options);

  /* Check return value: */
  if (ctx == NULL)
  {
    fprintf(stderr, "Cannot start CivetWeb - mg_start failed.\n");
    return EXIT_FAILURE;
  }

  mg_set_request_handler(ctx, EXIT_URI, ExitHandler, 0);
  // mg_set_request_handler(ctx, "/form", FileHandler, (void *)"README.md");
  // mg_set_request_handler(ctx, "/js/**.js$", AssetHandler, "html");
  mg_set_request_handler(ctx, "/", AssetHandler, (void *)"html");
  /* Show sone info */
  printf("Start example: %s%s\n", HOST_INFO, EXAMPLE_URI);
  printf("Exit example:  %s%s\n", HOST_INFO, EXIT_URI);
#ifdef USE_WEBSOCKET
  /* WS site for the websocket connection */
  mg_set_websocket_handler(ctx,
                           "/websocket",
                           WebSocketConnectHandler,
                           WebSocketReadyHandler,
                           WebsocketDataHandler,
                           WebSocketCloseHandler,
                           0);
#endif
  /* Wait until the server should be closed */
  while (!exitNow)
  {
#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif
#ifdef USE_WEBSOCKET
    InformWebsockets(ctx);
#endif
  }

  /* Stop the server */
  mg_stop(ctx);

  printf("Server stopped.\n");
  printf("Bye!\n");

  return EXIT_SUCCESS;
}