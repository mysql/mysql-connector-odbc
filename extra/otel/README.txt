How to update the bundled version of opentelemetry-cpp
======================================================

We use copy of opentelemetry-cpp sources that are bundled with the server
(note that server potentially patches the original sources). When server
starts to bundle new version X.Y.Z the following should be done to update
our copy:

1. Delete the current opentelemtetry-cpp-* subfolder from this folder.

2. Copy folder opentelemetry-cpp-X.Y.Z from the server source tree to this
  folder.

3. Inside opentelemtry-cpp-X.Y.Z delete everything except api/ subfolder
  and LICENSE file.

4. Inside CMakeLists.txt in this folder update OPENTELEMETRY_CPP_TAG
  variable:

  set(OPENTELEMETRY_CPP_TAG "opentelemetry-cpp-X.Y.Z" CACHE INTERNAL "")

3. Build and test connector on a platform on which we support OTel (Linux)
  to confirm that it works as before.
