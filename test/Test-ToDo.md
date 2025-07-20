# This file is a todoList for wd-broker tests and the test framework.

- Add a test to verify unregister behavior with differnt pids
  - When a client has been registered with pid A and then unregisters with pid B, it should not be removed
  - When a client has been registered with pid A and then unregisters with pid B but pid B is root, it should be removed

- Extend the framework to supported skipped test or be able to run test as certain user
  - This is needed as some test must run as root, some must not run as root