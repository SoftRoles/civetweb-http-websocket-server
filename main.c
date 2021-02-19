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

  /* Wait until the server should be closed */
  while (!exitNow)
  {
#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif
  }

  /* Stop the server */
  mg_stop(ctx);

  printf("Server stopped.\n");
  printf("Bye!\n");

  return EXIT_SUCCESS;
}