# Task 2.4: Single Track Performance Optimization - Beast Implementation Plan

## Overview
Optimize OscilloscopeComponent rendering to achieve:
- **CPU <1% single track at 60 FPS**
- **Zero per-frame heap allocations**
- **Stable 60 FPS with <2ms frame variance**
- **Optional decimation for high pixel density**

## Implementation Subtasks

### 2.4.1: Memory Allocation Optimization
**Priority**: CRITICAL
**Estimated Time**: 2 hours

**Issues to Fix**:
- Vector allocation in paint(): `std::vector<float> temp(N, 0.f)`
- Path object creation on every frame
- String allocations in debug output

**Implementation**:
- Add pre-allocated member vectors sized to max samples needed
- Cache Path objects and clear/reuse them
- Use stack-allocated arrays for small buffers
- Move debug output to conditional compilation

**Acceptance Criteria**:
- Zero heap allocations detected via profiler during normal rendering
- Memory usage remains constant during sustained rendering

### 2.4.2: Decimation/LOD System Implementation
**Priority**: HIGH
**Estimated Time**: 2 hours

**Requirements**:
- Implement min/max decimation when pixel density < 1 sample per pixel
- Fallback to no-op for small buffers
- Configurable decimation threshold

**Implementation**:
- Add DecimationProcessor class with min/max stride calculation
- Integrate into OscilloscopeComponent paint cycle
- Support variable window widths and zoom levels

**Acceptance Criteria**:
- Decimation activates automatically when samples > pixels
- Visual quality maintained while reducing vertex count
- Performance improvement measurable at high sample counts

### 2.4.3: Rendering Path Optimization
**Priority**: HIGH
**Estimated Time**: 1.5 hours

**Issues to Fix**:
- Dual rendering paths (bridge + legacy ringbuffer)
- Path vs direct line drawing decision
- Redundant bounds calculations

**Implementation**:
- Remove legacy ringbuffer fallback path
- Choose optimal rendering method based on sample count
- Cache bounds calculations
- Optimize color lookups

**Acceptance Criteria**:
- Single, optimized rendering path
- Rendering method chosen based on performance characteristics
- Measurable performance improvement in paint cycle

### 2.4.4: Performance Monitoring Infrastructure
**Priority**: HIGH
**Estimated Time**: 1 hour

**Requirements**:
- Frame timing measurement with statistics
- Paint cycle profiling
- Allocation tracking hooks
- Performance regression detection

**Implementation**:
- Add PerformanceMonitor class with timing measurement
- Integrate frame timing collection
- Add conditional debug output for performance metrics
- Create performance test automation

**Acceptance Criteria**:
- Frame timing data collection with min/max/avg/stddev
- Paint cycle timing breakdown available
- Performance metrics available for validation

### 2.4.5: Frame Pacing Optimization
**Priority**: MEDIUM
**Estimated Time**: 30 minutes

**Requirements**:
- Consistent frame timing
- Reduced frame variance to <2ms standard deviation
- Stable 60 FPS target

**Implementation**:
- Optimize timer callback frequency
- Minimize work in paint cycle
- Profile timer jitter

**Acceptance Criteria**:
- Frame pacing variance <2ms std dev over 10s sample
- Stable 60 FPS achieved consistently
- Timer overhead minimized

## Testing Strategy

### Performance Test Cases
1. **Zero Allocation Test**: Run for 60 seconds, verify no heap growth
2. **Frame Timing Test**: Measure 10s of frame times, verify <2ms std dev
3. **CPU Usage Test**: Monitor CPU usage, verify <1% single track
4. **Decimation Test**: Test with various window sizes and sample counts
5. **Regression Test**: Compare performance before/after optimizations

### Benchmarking Infrastructure
- Automated performance regression detection
- Comparative benchmarks (before/after)
- Memory usage monitoring
- CPU usage measurement

## Implementation Priority
1. **Memory allocation elimination** (highest impact)
2. **Performance monitoring** (enables measurement)
3. **Decimation system** (scalability for multi-track)
4. **Rendering path cleanup** (code quality + performance)
5. **Frame pacing** (polish for smooth experience)

## Success Metrics
- **Primary**: CPU <1% @ 60 FPS single track
- **Primary**: Zero allocations per frame
- **Primary**: Frame variance <2ms std dev
- **Secondary**: Decimation reduces vertex count appropriately
- **Secondary**: Performance monitoring provides actionable data
