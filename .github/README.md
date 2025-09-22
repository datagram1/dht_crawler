# CI/CD Pipeline Documentation

This directory contains the CI/CD pipeline configuration for the DHT Crawler project.

## Overview

The CI/CD pipeline provides automated building, testing, code quality checks, security scanning, and deployment for the DHT Crawler project.

## Pipeline Components

### 1. Build and Test
- **Platforms**: Ubuntu, macOS, Windows
- **Compilers**: GCC, Clang, MSVC
- **Build Types**: Release, Debug
- **Testing**: Unit tests, Integration tests, Performance tests

### 2. Code Quality
- **Formatting**: clang-format
- **Static Analysis**: cppcheck, clang-tidy
- **Code Coverage**: Coverage reports
- **Documentation**: Doxygen generation

### 3. Security Scanning
- **Vulnerability Scanning**: Trivy
- **Dependency Scanning**: Security audit
- **SAST**: Static Application Security Testing

### 4. Performance Testing
- **Benchmarks**: Performance benchmarks
- **Memory Analysis**: Valgrind
- **Profiling**: Performance profiling

### 5. Database Testing
- **Migration Testing**: Database migration tests
- **Integration Testing**: Database integration tests
- **Performance Testing**: Database performance tests

### 6. Deployment
- **Release Building**: Automated release builds
- **Package Creation**: Release packages
- **GitHub Releases**: Automated GitHub releases

## Workflow Files

### ci.yml
Main CI/CD workflow that runs on:
- Push to main/develop branches
- Pull requests to main/develop branches
- Release creation

### Features:
- Multi-platform builds (Ubuntu, macOS, Windows)
- Multi-compiler support (GCC, Clang, MSVC)
- Automated testing
- Code quality checks
- Security scanning
- Performance testing
- Database testing
- Automated deployment

## Docker Configuration

### Dockerfile.ci
CI-specific Dockerfile for building and testing:
- Ubuntu 22.04 base image
- All development dependencies
- Build tools and compilers
- Testing frameworks
- Code quality tools

### docker-compose.test.yml
Test environment with:
- MySQL database
- DHT Crawler application
- Performance testing
- Database migration testing

## Scripts

### ci_setup.sh
CI/CD setup and execution script:
- Environment setup
- Dependency installation
- Docker configuration
- Build execution
- Test execution
- Code quality checks
- Security scanning
- Performance analysis
- Database testing
- Report generation

## Usage

### Local Development
```bash
# Set up CI/CD environment
./scripts/ci_setup.sh --setup

# Run all CI/CD steps
./scripts/ci_setup.sh --all

# Run specific steps
./scripts/ci_setup.sh --build --test --quality
```

### GitHub Actions
The pipeline runs automatically on:
- Push to main/develop branches
- Pull requests
- Release creation

### Docker Testing
```bash
# Run tests with Docker
docker-compose -f docker-compose.test.yml up --build

# Run specific tests
docker-compose -f docker-compose.test.yml run --rm dht_crawler
```

## Configuration

### Environment Variables
- `BUILD_TYPE`: Build type (Release/Debug)
- `CMAKE_BUILD_TYPE`: CMake build type
- `CC`: C compiler
- `CXX`: C++ compiler

### Build Configuration
- CMake build system
- Multi-platform support
- Parallel builds
- Optimized builds

### Test Configuration
- Google Test framework
- Coverage reporting
- Performance testing
- Integration testing

## Monitoring

### Build Status
- GitHub Actions status badges
- Build notifications
- Test result reporting

### Performance Metrics
- Build time tracking
- Test execution time
- Performance benchmarks
- Resource usage

### Quality Metrics
- Code coverage
- Static analysis results
- Security scan results
- Performance metrics

## Troubleshooting

### Common Issues

1. **Build Failures**
   - Check compiler compatibility
   - Verify dependency versions
   - Check build configuration

2. **Test Failures**
   - Check test environment
   - Verify database connectivity
   - Check test data

3. **Quality Check Failures**
   - Fix code formatting issues
   - Address static analysis warnings
   - Improve code coverage

4. **Security Scan Failures**
   - Update vulnerable dependencies
   - Fix security issues
   - Review security policies

### Debugging

1. **Local Debugging**
   ```bash
   # Run with debug output
   ./scripts/ci_setup.sh --build --test --verbose
   
   # Run specific tests
   ctest --test-dir build --output-on-failure -R specific_test
   ```

2. **Docker Debugging**
   ```bash
   # Run with debug output
   docker-compose -f docker-compose.test.yml up --build --verbose
   
   # Access container shell
   docker-compose -f docker-compose.test.yml exec dht_crawler bash
   ```

3. **GitHub Actions Debugging**
   - Check workflow logs
   - Review build artifacts
   - Check environment variables

## Best Practices

### Development
- Write comprehensive tests
- Follow coding standards
- Use proper error handling
- Document code changes

### Testing
- Test all code paths
- Use realistic test data
- Test edge cases
- Performance test critical paths

### Security
- Regular dependency updates
- Security scan integration
- Secure coding practices
- Vulnerability management

### Performance
- Regular performance testing
- Monitor resource usage
- Optimize critical paths
- Performance regression testing

## Support

For CI/CD issues:
1. Check the workflow logs
2. Review the configuration
3. Test locally first
4. Check dependencies
5. Review documentation

## Contributing

When contributing to CI/CD:
1. Test changes locally
2. Update documentation
3. Follow best practices
4. Review security implications
5. Test on multiple platforms
