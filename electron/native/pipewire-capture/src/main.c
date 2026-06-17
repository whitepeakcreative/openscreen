#include <glib.h>
#include <gio/gio.h>
#include <gst/gst/gst.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// JSON helpers (same approach as the Windows WGC helper)
// ---------------------------------------------------------------------------

static gchar *findString(const gchar *json, const gchar *key) {
	gchar *needle = g_strdup_printf("\"%s\"", key);
	const gchar *pos = strstr(json, needle);
	g_free(needle);
	if (!pos) return NULL;

	pos = strchr(pos + strlen(key) + 2, ':');
	if (!pos) return NULL;
	pos++;
	while (*pos == ' ' || *pos == '\t') pos++;
	if (*pos != '"') return NULL;
	pos++;

	GString *result = g_string_new(NULL);
	while (*pos && *pos != '"') {
		if (*pos == '\\' && pos[1]) {
			pos++;
			switch (*pos) {
				case 'n': g_string_append_c(result, '\n'); break;
				case 'r': g_string_append_c(result, '\r'); break;
				case 't': g_string_append_c(result, '\t'); break;
				case '"': g_string_append_c(result, '"'); break;
				case '\\': g_string_append_c(result, '\\'); break;
				default: g_string_append_c(result, *pos); break;
			}
		} else {
			g_string_append_c(result, *pos);
		}
		pos++;
	}
	return g_string_free(result, FALSE);
}

static gboolean findBool(const gchar *json, const gchar *key, gboolean fallback) {
	gchar *needle = g_strdup_printf("\"%s\"", key);
	const gchar *pos = strstr(json, needle);
	g_free(needle);
	if (!pos) return fallback;

	pos = strchr(pos + strlen(key) + 2, ':');
	if (!pos) return fallback;
	pos++;
	while (*pos == ' ' || *pos == '\t') pos++;

	if (strncmp(pos, "true", 4) == 0) return TRUE;
	if (strncmp(pos, "false", 5) == 0) return FALSE;
	return fallback;
}

static gint64 findInt64(const gchar *json, const gchar *key, gint64 fallback) {
	gchar *needle = g_strdup_printf("\"%s\"", key);
	const gchar *pos = strstr(json, needle);
	g_free(needle);
	if (!pos) return fallback;

	pos = strchr(pos + strlen(key) + 2, ':');
	if (!pos) return fallback;
	pos++;
	while (*pos == ' ' || *pos == '\t') pos++;

	gchar *end = NULL;
	gint64 val = g_ascii_strtoll(pos, &end, 10);
	if (end == pos) return fallback;
	return val;
}

static gint findInt(const gchar *json, const gchar *key, gint fallback) {
	return (gint)findInt64(json, key, fallback);
}

static gchar *jsonEscape(const gchar *value) {
	GString *s = g_string_new(NULL);
	for (const gchar *p = value; *p; p++) {
		switch (*p) {
			case '\\': g_string_append(s, "\\\\"); break;
			case '"': g_string_append(s, "\\\""); break;
			case '\n': g_string_append(s, "\\n"); break;
			case '\r': g_string_append(s, "\\r"); break;
			case '\t': g_string_append(s, "\\t"); break;
			default: g_string_append_c(s, *p); break;
		}
	}
	return g_string_free(s, FALSE);
}

// ---------------------------------------------------------------------------
// Event emission
// ---------------------------------------------------------------------------

static void emit(const gchar *json) {
	g_print("%s\n", json);
	fflush(stdout);
}

static void emitError(const gchar *code, const gchar *message) {
	gchar *escaped = jsonEscape(message);
	gchar *json = g_strdup_printf(
		"{\"event\":\"error\",\"code\":\"%s\",\"message\":\"%s\"}",
		code, escaped);
	emit(json);
	g_free(json);
	g_free(escaped);
}

// ---------------------------------------------------------------------------
// Portal request: synchronous wrapper around the async xdg-desktop-portal API
// ---------------------------------------------------------------------------

typedef struct {
	GMainLoop *loop;
	guint response_code;
	GVariant *results;
	gboolean got_response;
} PortalRequest;

static void onPortalResponse(
	G_GNUC_UNUSED GDBusConnection *conn,
	G_GNUC_UNUSED const gchar *sender,
	G_GNUC_UNUSED const gchar *path,
	G_GNUC_UNUSED const gchar *iface,
	G_GNUC_UNUSED const gchar *signal,
	GVariant *params,
	gpointer user_data)
{
	PortalRequest *req = (PortalRequest *)user_data;
	guint response;
	GVariant *results;
	g_variant_get(params, "(u@a{sv})", &response, &results);
	req->response_code = response;
	req->results = results;
	req->got_response = TRUE;
	g_main_loop_quit(req->loop);
}

static gboolean timeoutCb(gpointer user_data) {
	g_main_loop_quit((GMainLoop *)user_data);
	return G_SOURCE_REMOVE;
}

// Call a portal method and wait for the Response signal.
// Returns the results GVariant (caller unrefs) or NULL on failure.
static GVariant *portalCall(
	GDBusProxy *proxy,
	GMainLoop *loop,
	GDBusConnection *conn,
	const gchar *method,
	GVariant *params,
	gchar **error_msg)
{
	GError *error = NULL;
	GVariant *ret = g_dbus_proxy_call_sync(
		proxy, method, params, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
	if (!ret) {
		*error_msg = g_strdup(error->message);
		g_error_free(error);
		return NULL;
	}

	const gchar *request_path = NULL;
	g_variant_get(ret, "(o)", &request_path);
	if (!request_path) {
		g_variant_unref(ret);
		*error_msg = g_strdup("Portal returned empty request path");
		return NULL;
	}

	PortalRequest req = { .loop = loop, .got_response = FALSE, .results = NULL };
	guint sub_id = g_dbus_connection_signal_subscribe(
		conn, NULL,
		"org.freedesktop.portal.Request",
		"Response",
		request_path,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		onPortalResponse,
		&req,
		NULL);

	// 30-second timeout
	guint timeout_id = g_timeout_add_seconds(30, timeoutCb, loop);

	g_main_loop_run(loop);

	if (timeout_id) g_source_remove(timeout_id);
	g_dbus_connection_signal_unsubscribe(conn, sub_id);
	g_variant_unref(ret);

	if (!req.got_response) {
		*error_msg = g_strdup("Portal request timed out");
		return NULL;
	}

	if (req.response_code != 0) {
		*error_msg = g_strdup_printf("Portal request failed with response code %u", req.response_code);
		if (req.results) g_variant_unref(req.results);
		return NULL;
	}

	return req.results;
}

// ---------------------------------------------------------------------------
// GStreamer pipeline
// ---------------------------------------------------------------------------

typedef struct {
	GstElement *pipeline;
	GMainLoop *loop;
	gboolean stopRequested;
	gboolean recordingStarted;
	GIOChannel *stdinChannel;
	guint stdinWatch;
	gchar *outputPath;
} CaptureState;

static gboolean onBusMessage(
	G_GNUC_UNUSED GstBus *bus,
	GstMessage *msg,
	gpointer data)
{
	CaptureState *state = (CaptureState *)data;

	switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR: {
			gchar *debug = NULL;
			GError *error = NULL;
			gst_message_parse_error(msg, &error, &debug);
			gchar *errMsg = g_strdup_printf("GStreamer: %s", error->message);
			emitError("gstreamer-error", errMsg);
			g_free(errMsg);
			g_error_free(error);
			g_free(debug);
			g_main_loop_quit(state->loop);
			break;
		}
		case GST_MESSAGE_EOS:
			g_main_loop_quit(state->loop);
			break;
		default:
			break;
	}
	return TRUE;
}

static gboolean onStdin(GIOChannel *source,
	G_GNUC_UNUSED GIOCondition condition,
	gpointer data)
{
	CaptureState *state = (CaptureState *)data;
	gchar *line = NULL;
	gsize length = 0;
	GError *error = NULL;

	GIOStatus status = g_io_channel_read_line(source, &line, &length, NULL, &error);
	if (status == G_IO_STATUS_NORMAL) {
		g_strstrip(line);
		if (g_strcmp0(line, "stop") == 0 || g_strcmp0(line, "q") == 0 || g_strcmp0(line, "quit") == 0) {
			state->stopRequested = TRUE;
			gst_element_send_event(state->pipeline, gst_event_new_eos());
		}
		g_free(line);
	} else if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR) {
		state->stopRequested = TRUE;
		gst_element_send_event(state->pipeline, gst_event_new_eos());
	}
	if (error) g_error_free(error);
	return TRUE;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
	if (argc < 2) {
		g_printerr("ERROR: Missing JSON config argument\n");
		return 1;
	}

	const gchar *configJson = argv[1];

	// Parse config
	gchar *outputPath = findString(configJson, "screenPath");
	if (!outputPath || !*outputPath) {
		g_free(outputPath);
		outputPath = findString(configJson, "outputPath");
	}
	if (!outputPath || !*outputPath) {
		g_printerr("ERROR: Missing outputPath in config\n");
		g_free(outputPath);
		return 1;
	}

	gint fps = findInt(configJson, "fps", 60);
	if (fps < 1) fps = 60;
	if (fps > 120) fps = 120;

	gint width = findInt(configJson, "videoWidth", findInt(configJson, "width", 0));
	gint height = findInt(configJson, "videoHeight", findInt(configJson, "height", 0));
	gboolean captureSystemAudio = findBool(configJson, "captureSystemAudio", FALSE);

	// Emit ready immediately
	emit("{\"event\":\"ready\",\"schemaVersion\":2}");

	// Initialize GStreamer
	gst_init(&argc, &argv);

	// Check required elements
	const gchar *missing = NULL;
	if (!gst_element_factory_find("pipewiresrc")) missing = "pipewiresrc (gst-plugins-bad)";
	if (!gst_element_factory_find("x264enc")) missing = missing ? missing : "x264enc (gst-plugins-ugly)";
	if (!gst_element_factory_find("mp4mux")) missing = missing ? missing : "mp4mux (gst-plugins-good)";
	if (missing) {
		gchar *msg = g_strdup_printf("Required GStreamer element not found: %s", missing);
		emitError("missing-gst-element", msg);
		g_free(msg);
		g_free(outputPath);
		return 1;
	}

	// Find an AAC encoder
	const gchar *aacEnc = NULL;
	if (gst_element_factory_find("avenc_aac")) aacEnc = "avenc_aac";
	else if (gst_element_factory_find("voaacenc")) aacEnc = "voaacenc";
	else if (gst_element_factory_find("fdkaacenc")) aacEnc = "fdkaacenc";

	// Connect to D-Bus session bus
	GError *error = NULL;
	GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (!conn) {
		emitError("dbus-connect", error->message);
		g_error_free(error);
		g_free(outputPath);
		return 1;
	}

	// Create proxy for the ScreenCast portal
	GDBusProxy *proxy = g_dbus_proxy_new_sync(
		conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.ScreenCast",
		NULL, &error);
	if (!proxy) {
		emitError("portal-proxy", error->message);
		g_error_free(error);
		g_object_unref(conn);
		g_free(outputPath);
		return 1;
	}

	// Check AvailableCursorModes for hidden (bit 0)
	GVariant *cursorModesVar = g_dbus_proxy_get_cached_property(proxy, "AvailableCursorModes");
	if (cursorModesVar) {
		guint32 cursorModes = g_variant_get_uint32(cursorModesVar);
		g_variant_unref(cursorModesVar);
		if (!(cursorModes & 1u)) {
			emitError("cursor-hidden-unsupported",
				"This compositor does not support hiding the cursor during capture");
			g_object_unref(proxy);
			g_object_unref(conn);
			g_free(outputPath);
			return 1;
		}
	}

	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	gchar *errorMsg = NULL;

	// 1. CreateSession
	GVariantBuilder *csBuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(csBuilder, "{sv}", "session_handle_token",
		g_variant_new_string("openscreen_session"));
	GVariant *csParams = g_variant_new("(a{sv})", csBuilder);
	g_variant_builder_unref(csBuilder);

	GVariant *csResults = portalCall(proxy, loop, conn, "CreateSession", csParams, &errorMsg);
	if (!csResults) {
		emitError("create-session", errorMsg);
		g_free(errorMsg);
		goto cleanup;
	}

	const gchar *sessionHandle = NULL;
	g_variant_lookup(csResults, "session_handle", "&s", &sessionHandle);
	if (!sessionHandle) {
		emitError("create-session", "No session_handle in response");
		g_variant_unref(csResults);
		goto cleanup;
	}

	// 2. SelectSources (cursor_mode=1=hidden, types=1=monitor)
	GVariantBuilder *ssBuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(ssBuilder, "{sv}", "types", g_variant_new_uint32(1));
	g_variant_builder_add(ssBuilder, "{sv}", "multiple", g_variant_new_boolean(FALSE));
	g_variant_builder_add(ssBuilder, "{sv}", "cursor_mode", g_variant_new_uint32(1));
	GVariant *ssParams = g_variant_new("(oa{sv})",
		g_variant_new_object_path(sessionHandle), ssBuilder);
	g_variant_builder_unref(ssBuilder);

	GVariant *ssResults = portalCall(proxy, loop, conn, "SelectSources", ssParams, &errorMsg);
	if (!ssResults) {
		emitError("select-sources", errorMsg);
		g_free(errorMsg);
		g_variant_unref(csResults);
		goto cleanup;
	}
	g_variant_unref(ssResults);

	// 3. Start
	GVariant *startParams = g_variant_new("(os)",
		g_variant_new_object_path(sessionHandle), "");
	GVariant *startResults = portalCall(proxy, loop, conn, "Start", startParams, &errorMsg);
	if (!startResults) {
		emitError("start", errorMsg);
		g_free(errorMsg);
		g_variant_unref(csResults);
		goto cleanup;
	}

	// Extract node_id from streams array
	GVariant *streams = g_variant_lookup_value(startResults, "streams", NULL);
	if (!streams || g_variant_n_children(streams) == 0) {
		emitError("start", "No streams in Start response");
		if (streams) g_variant_unref(streams);
		g_variant_unref(startResults);
		g_variant_unref(csResults);
		goto cleanup;
	}

	GVariant *firstStream = g_variant_get_child_value(streams, 0);
	guint32 nodeId = 0;
	GVariant *streamProps = NULL;
	g_variant_get(firstStream, "(u@a{sv})", &nodeId, &streamProps);

	// Get stream size if available
	gint streamWidth = width;
	gint streamHeight = height;
	if (streamProps) {
		GVariant *sizeVar = g_variant_lookup_value(streamProps, "size", NULL);
		if (sizeVar) {
			gint32 sw, sh;
			g_variant_get(sizeVar, "(ii)", &sw, &sh);
			if (streamWidth <= 0) streamWidth = sw;
			if (streamHeight <= 0) streamHeight = sh;
			g_variant_unref(sizeVar);
		}
		g_variant_unref(streamProps);
	}

	g_variant_unref(firstStream);
	g_variant_unref(streams);

	// Build GStreamer pipeline
	gchar *nodeIdStr = g_strdup_printf("%u", nodeId);
	gint bitrate = 18000000;
	if (streamWidth > 0 && streamHeight > 0) {
		gint64 pixels = (gint64)streamWidth * streamHeight;
		if (pixels >= 3840 * 2160) bitrate = 45000000;
		else if (pixels >= 2560 * 1440) bitrate = 28000000;
	}

	GString *pipeStr = g_string_new(NULL);

	if (captureSystemAudio && aacEnc) {
		g_string_printf(pipeStr,
			"pulsesrc device=@DEFAULT_MONITOR@ ! audioconvert ! %s bitrate=192000 ! queue ! "
			"mp4mux name=mux location=\"%s\" "
			"pipewiresrc path=%s ! videoconvert ! videoscale ! "
			"video/x-raw,width=%d,height=%d ! videorate ! "
			"video/x-raw,framerate=%d/1 ! x264enc bitrate=%d speed-preset=ultrafast "
			"tune=zerolatency key-int-max=%d ! queue ! mux.",
			aacEnc, outputPath, nodeIdStr,
			streamWidth > 0 ? streamWidth : 1920,
			streamHeight > 0 ? streamHeight : 1080,
			fps, bitrate, fps);
	} else {
		g_string_printf(pipeStr,
			"pipewiresrc path=%s ! videoconvert ! videoscale ! "
			"video/x-raw,width=%d,height=%d ! videorate ! "
			"video/x-raw,framerate=%d/1 ! x264enc bitrate=%d speed-preset=ultrafast "
			"tune=zerolatency key-int-max=%d ! queue ! mp4mux location=\"%s\"",
			nodeIdStr,
			streamWidth > 0 ? streamWidth : 1920,
			streamHeight > 0 ? streamHeight : 1080,
			fps, bitrate, fps, outputPath);
	}

	g_free(nodeIdStr);

	// Parse and build pipeline
	GError *gstError = NULL;
	GstElement *pipeline = gst_parse_launch(pipeStr->str, &gstError);
	g_string_free(pipeStr, TRUE);

	if (gstError || !pipeline) {
		gchar *msg = gstError ? g_strdup(gstError->message) : g_strdup("Unknown parse error");
		emitError("pipeline-parse", msg);
		g_free(msg);
		if (gstError) g_error_free(gstError);
		g_variant_unref(startResults);
		g_variant_unref(csResults);
		goto cleanup;
	}

	// Set up bus watch
	GstBus *bus = gst_element_get_bus(pipeline);
	gst_bus_add_signal_watch(bus);

	CaptureState state = {
		.pipeline = pipeline,
		.loop = loop,
		.stopRequested = FALSE,
		.recordingStarted = FALSE,
		.outputPath = outputPath,
	};

	g_signal_connect(bus, "message", G_CALLBACK(onBusMessage), &state);
	gst_object_unref(bus);

	// Set up stdin command handling
	state.stdinChannel = g_io_channel_unix_new(STDIN_FILENO);
	state.stdinWatch = g_io_add_watch(state.stdinChannel,
		G_IO_IN, onStdin, &state);

	// Start pipeline
	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	// Emit recording-started
	emit("{\"event\":\"recording-started\",\"schemaVersion\":2}");

	// Run main loop
	g_main_loop_run(loop);

	// Stop pipeline
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);

	// Clean up stdin
	if (state.stdinWatch) g_source_remove(state.stdinWatch);
	if (state.stdinChannel) g_io_channel_unref(state.stdinChannel);

	// Emit recording-stopped
	{
		gchar *escapedPath = jsonEscape(outputPath);
		gchar *json = g_strdup_printf(
			"{\"event\":\"recording-stopped\",\"schemaVersion\":2,\"screenPath\":\"%s\"}",
			escapedPath);
		emit(json);
		g_free(json);
		g_free(escapedPath);
	}

	g_variant_unref(startResults);
	g_variant_unref(csResults);

cleanup:
	g_main_loop_unref(loop);
	g_object_unref(proxy);
	g_object_unref(conn);
	g_free(outputPath);
	return 0;
}
