// files — house photos and the layout file for the House view editor (editor_page.h).
//
// Everything lives in /files on the FAT storage area and is served as-is at /files/...
//   POST /api/upload        multipart form with one file (the browser shrinks photos first);
//                           saved under its own file name, max FILE_MAX_BYTES
//   POST /api/layout        body = layout JSON (written by the editor; the board only
//                           stores it, it never reads it) -> /files/layout.json
//   POST /api/delete-file   name=<file> removes /files/<file>
//   GET  /files/<file>      photos and layout.json
//
// File names are limited to letters, digits, '-', '_' and '.', and must end in
// .jpg, .jpeg or .png (photos) — the layout is always layout.json.

const char*    FILES_DIR      = "/files";
const char*    LAYOUT_FILE    = "/files/layout.json";
const size_t   FILE_MAX_BYTES = 4UL * 1024 * 1024;
const size_t   LAYOUT_MAX     = 128UL * 1024;

File   uploadFile;
bool   uploadOk = false;
String uploadPath;
size_t uploadBytes = 0;

void startFiles() {
  if (storageOk && !FFat.exists(FILES_DIR)) FFat.mkdir(FILES_DIR);
}

// Cleans a file name; returns "" if it isn't an allowed photo name.
String safePhotoName(const String& raw) {
  String name;
  for (unsigned int i = 0; i < raw.length() && name.length() < 48; i++) {
    char c = raw[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') name += (char)tolower(c);
  }
  if (name.startsWith(".")) return "";
  if (!(name.endsWith(".jpg") || name.endsWith(".jpeg") || name.endsWith(".png"))) return "";
  return name;
}

// Called repeatedly while a file arrives.
void handleUploadData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    String name = safePhotoName(up.filename);
    uploadOk = storageOk && name.length() > 0;
    uploadBytes = 0;
    uploadPath = String(FILES_DIR) + "/" + name;
    if (uploadOk) {
      uploadFile = FFat.open(uploadPath, "w");
      uploadOk = (bool)uploadFile;
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadOk) {
      uploadBytes += up.currentSize;
      if (uploadBytes > FILE_MAX_BYTES || uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
        uploadOk = false;
      }
    }
  } else if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    if (up.status == UPLOAD_FILE_ABORTED) uploadOk = false;
    if (!uploadOk && uploadPath.length() && FFat.exists(uploadPath)) FFat.remove(uploadPath);
  }
}

// Called once the whole request has arrived.
void handleUploadDone() {
  if (!storageOk) return sendError("storage not available");
  if (!uploadOk) return sendError("upload failed: photo must be .jpg/.png under 4 MB");
  Serial.printf("Files: saved %s (%u KB)\n", uploadPath.c_str(), (unsigned)(uploadBytes / 1024));
  String body = "{\"ok\":true,\"path\":\"" + uploadPath + "\",\"bytes\":" + String(uploadBytes) + "}";
  server.send(200, "application/json", body);
}

void handleSaveLayout() {
  if (!storageOk) return sendError("storage not available");
  const String& body = server.arg("plain");
  if (body.length() == 0 || body.length() > LAYOUT_MAX || body[0] != '{') {
    return sendError("layout must be a JSON object under 128 KB");
  }
  File f = FFat.open(LAYOUT_FILE, "w");
  if (!f) return sendError("could not write layout");
  f.print(body);
  f.close();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteFile() {
  String name = safePhotoName(server.arg("name"));
  if (!name.length()) return sendError("bad file name");
  String path = String(FILES_DIR) + "/" + name;
  if (FFat.exists(path)) FFat.remove(path);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleEditor() {
  server.send_P(200, "text/html", EDITOR_HTML);
}

// Registers the file routes on the web server (called from startWeb()).
void addFileRoutes() {
  server.on("/editor", HTTP_GET, handleEditor);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadData);
  server.on("/api/layout", HTTP_POST, handleSaveLayout);
  server.on("/api/delete-file", HTTP_POST, handleDeleteFile);
  if (storageOk) server.serveStatic("/files", FFat, FILES_DIR, "no-cache");
}
