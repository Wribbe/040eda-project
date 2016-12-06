Compiling and installing camera software on camera:

  1.) Run 'make both' to build both host-specific and embedded executables.
  2.) Run 'make push' to initialize a push of the embedded executable to a
      argus-x camera.
  3.) ssh into the camera used in the previous step.
  4.) Run './edan040_server' to start the server.
