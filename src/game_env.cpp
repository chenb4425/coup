#include <pybind11/pybind11.h>
#include "game_logic.h"

//This allows us to connect our C++ code to python via pybind

namespace py = pybind11;

//I barely understand how this works
PYBIND11_MODULE(game_env, m) {
    py::class_<GameState>(m, "GameState")
        .def(py::init<>())
        .def_readwrite("current_player", &GameState::current_player);

    m.def("reset", &reset);
    //m.def("step", &step);
}
