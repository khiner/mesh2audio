!> \\file testModule.f
program p_main

    use m_global_parameters
    use m_data_input_output

    implicit none

    call s_read_mesh_file()

end program p_main