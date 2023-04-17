#!/bin/bash
gfortran -c src/precmod.f90 -o precmod.o
gfortran -c src/stringmod.f90 -o stringmod.o
gfortran -c src/m_global_parameters.f90 -o m_global_parameters.o
gfortran -c src/m_math.f90 -o m_math.o
gfortran -c src/m_data_input_output.f90 -o m_data_input_output.o
gfortran -c src/m_finite_elements.f90 -o m_finite_elements.o
gfortran -c src/p_main.f90 -o p_main.o


gfortran precmod.o stringmod.o m_global_parameters.o m_math.o m_data_input_output.o m_finite_elements.o p_main.o -o fem

rm -rf *.mod *.o src/*.mod src/*.o
