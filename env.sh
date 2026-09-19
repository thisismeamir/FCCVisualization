# FCCVisualization
export FCCVIS_DIR="$HOME/projects/pandora-xyz/FCCVisualization"
export FCCVIS_BUILD="$FCCVIS_DIR/build"

export LD_LIBRARY_PATH="$FCCVIS_BUILD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$FCCVIS_BUILD/bin${PATH:+:$PATH}"
export CMAKE_PREFIX_PATH="$FCCVIS_BUILD${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"

# Dependencies
source "$HOME/scientific-software/installs/root-6.38.00/bin/thisroot.sh"
source "$HOME/scientific-software/envs/pandora-sdk.sh"
source "$HOME/scientific-software/envs/edm4hep.sh"
source "$HOME/scientific-software/envs/podio.sh"


# Development helpers
b() {
    cmake --build "$FCCVIS_BUILD" -j"$(nproc)"
}

r() {
    cmake --build "$FCCVIS_BUILD" --clean-first -j"$(nproc)"
}

t() {
    ctest --test-dir "$FCCVIS_BUILD" --output-on-failure
}

rt() {
    root -l
}

c() {
    cd "$FCCVIS_DIR"
}

cb() {
    cd "$FCCVIS_BUILD"
}

e() {
    "$EDITOR" "$FCCVIS_DIR"
}
