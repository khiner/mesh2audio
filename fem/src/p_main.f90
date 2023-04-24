!> \\file testModule.f
program p_main

    ! Dependencies
    use m_global_parameters
    use m_data_input_output
    use m_finite_elements
    use m_math

    implicit none

    real(kind(0d0)) :: start, finish

    call cpu_time(start)

    call s_initialize_guass_quadrature()

    call s_read_obj_file()

    call s_initialize_d()

    call s_compute_global_stiffness_matrix()

    call s_compute_global_mass_matrix()

    call s_write_output()
    
    call cpu_time(finish)
    print*, "Run Time: ",finish - start
    print*, ""

end program p_main