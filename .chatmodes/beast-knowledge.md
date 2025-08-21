# BEAST KNOWLEDGE - Test Updates for New Structure

## Assumptions
- Tests currently include headers using old paths
- RingBuffer tests may need dsp/ path updates
- Test build system uses same include directories as main plugin
- Test failures during build are likely due to include path issues

## Decisions
- Update test include paths to match new structure
- Keep test functionality unchanged, only fix paths
- Maintain existing test coverage and validation
- Use relative paths from tests/ directory to src/

## Context Notes
- Tests currently fail to compile due to moved headers
- Main plugin builds successfully with new structure
- Need to check if tests have any direct dependencies on moved files
- CMakeLists.txt may need test include path updates

## Sources
- Current test files in tests/ directory
- CMakeLists.txt test configuration
- Architecture.md directory structure specification
