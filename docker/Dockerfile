# ── Runtime image ─────────────────────────────────────────────────────────────
# eclipse-temurin:21-jdk provides libjvm.so (required at runtime via JNI)
# and the full JDK needed for Java 21 Lambda functions.
FROM eclipse-temurin:21-jdk-noble

# ── System runtimes for Node.js and Python Lambdas ────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl python3 python3-pip \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

# ── Directory layout (matches defaults in main.cpp) ───────────────────────────
RUN mkdir -p \
        /usr/local/awsmock/bin \
        /usr/local/awsmock/lib/java \
        /usr/local/awsmock/etc \
        /usr/local/awsmock/log

# ── awsmock-grt executable ────────────────────────────────────────────────────
COPY cmake-build-release/awsmock-grt /usr/local/awsmock/bin/awsmock-grt
RUN chmod +x /usr/local/awsmock/bin/awsmock-grt

# ── AWS Lambda Java runtime JARs (aws-lambda-java-runtime-*, etc.) ────────────
# Drop any required JARs into lib/java/ on the host before building.
COPY lib/java/ /usr/local/awsmock/lib/java/

# ── Default configuration ─────────────────────────────────────────────────────
COPY docker/awsmock.json /usr/local/awsmock/etc/awsmock.json

# ── Runtime environment ───────────────────────────────────────────────────────
ENV PATH="/usr/local/awsmock/bin:${PATH}"
# JVM locates libjvm.so via JAVA_HOME set by the base image; expose it
# explicitly so the dynamic linker finds it without ldconfig.
ENV LD_LIBRARY_PATH="${JAVA_HOME}/lib/server:${LD_LIBRARY_PATH}"

EXPOSE 8080

ENTRYPOINT ["/usr/local/awsmock/bin/awsmock-grt"]
