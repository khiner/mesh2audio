!> \\file testModule.f
program p_main

    ! Dependencies
    use m_global_parameters
    use m_data_input_output
    use m_finite_elements
    use m_math

    implicit none

    !ym = 190*1e9
    ym = 10
    nu = 0.3

    call s_initialize_guass_quadrature()
    call s_initialize_d()

    call s_read_obj_file()

    call s_compute_global_stiffness_matrix()
    
    call s_finalize_program()

end program p_main