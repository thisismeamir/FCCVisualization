#!/usr/bin/env bash

# ============================================================================
# FCCVisualization Development Environment
# ============================================================================

export FCCVIS_DIR="$HOME/projects/pandora-xyz/FCCVisualization"
export FCCVIS_BUILD_DIR="$FCCVIS_DIR/build"
export FCCVIS_INCLUDE_DIR="$FCCVIS_BUILD_DIR/include"
export FCCVIS_LIB_DIR="$FCCVIS_BUILD_DIR/lib"
export FCCVIS_BIN_DIR="$FCCVIS_BUILD_DIR/bin"

# ----------------------------------------------------------------------------
# Runtime / build paths
# ----------------------------------------------------------------------------

export PATH="$FCCVIS_BIN_DIR:$PATH"
export LD_LIBRARY_PATH="$FCCVIS_LIB_DIR:${LD_LIBRARY_PATH:-}"
export CMAKE_PREFIX_PATH="$FCCVIS_BUILD_DIR:${CMAKE_PREFIX_PATH:-}"
export CPATH="$FCCVIS_INCLUDE_DIR:${CPATH:-}"
export LIBRARY_PATH="$FCCVIS_LIB_DIR:${LIBRARY_PATH:-}"

# ----------------------------------------------------------------------------
# ROOT / FCC dependencies
# ----------------------------------------------------------------------------

source "$HOME/scientific-software/installs/root-6.38.00/bin/thisroot.sh"
source "$HOME/scientific-software/envs/pandora-sdk.sh"
source "$HOME/scientific-software/envs/edm4hep.sh"
source "$HOME/scientific-software/envs/podio.sh"
source "$HOME/scientific-software/envs/dd4hep.sh"

# ----------------------------------------------------------------------------
# Navigation
# ----------------------------------------------------------------------------

alias cc='cd $FCCVIS_DIR'
alias cb='cd $FCCVIS_BUILD_DIR'
alias cs='cd $FCCVIS_DIR/src'
alias docs='cd $FCCVIS_DIR/docs'

# ----------------------------------------------------------------------------
# CMake / build
# ----------------------------------------------------------------------------

alias cm='cmake -S "$FCCVIS_DIR" -B "$FCCVIS_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug'
alias gb='cmake --build "$FCCVIS_BUILD_DIR" -j$(nproc)'
alias gm='cm && gb'
alias gc='rm -rf "$FCCVIS_BUILD_DIR"'
alias gi='cmake --install "$FCCVIS_BUILD_DIR"'

# ----------------------------------------------------------------------------
# Testing
# ----------------------------------------------------------------------------

alias gt='ctest --test-dir "$FCCVIS_BUILD_DIR" --output-on-failure'

# ----------------------------------------------------------------------------
# Run / development
# ----------------------------------------------------------------------------

alias fv='fccvis'
alias gr='root'
alias rt='root -l'

# ----------------------------------------------------------------------------
# Git
# ----------------------------------------------------------------------------

alias gs='git -C "$FCCVIS_DIR" status'
alias gl='git -C "$FCCVIS_DIR" log --oneline --decorate -20'
alias gd='git -C "$FCCVIS_DIR" diff'
alias gp='git -C "$FCCVIS_DIR" pull'
alias gP='git -C "$FCCVIS_DIR" push'

# ----------------------------------------------------------------------------
# Environment
# ----------------------------------------------------------------------------

alias ge='env | grep -E "^(FCCVIS|ROOTSYS|CMAKE_PREFIX_PATH|LD_LIBRARY_PATH)"'
