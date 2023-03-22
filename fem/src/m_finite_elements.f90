module m_finite_elements

    !< dependencies
    use m_global_parameters
    use m_math

    implicit none

    private; public :: s_initialize_guass_quadrature, &
        s_compute_element_stiffness_matrix, &
        s_compute_global_stiffness_matrix

    real(kind(0d0)), dimension(4) :: gx !< x guass-quadrature points
    real(kind(0d0)), dimension(4) :: gy !< y guass-quadrature points
    real(kind(0d0)), dimension(4) :: weights !< guass-quadrature weights

    real(kind(0d0)) :: pi = 3.141592653589793d0

contains

    subroutine s_initialize_guass_quadrature()

        real(kind(0d0)) :: c = 3d0
        gx(1) = -sqrt(c)/c; gx(2) = -sqrt(c)/c; gx(3) = sqrt(c)/c; gx(4) = sqrt(c)/c
        gy(1) = -sqrt(c)/c; gy(2) = sqrt(c)/c; gy(3) = -sqrt(c)/c; gy(4) = sqrt(c)/c
        weights(1) = 1d0; weights(2) = 1d0; weights(3) = 1d0; weights(4) = 1d0

    end subroutine s_initialize_guass_quadrature

    subroutine S_compute_global_stiffness_matrix()

        integer :: i, j, k, u, v
        real(kind(0d0)), dimension(6,6) :: KL !< element stiffness matrix 

        do i = 1, elements
            call s_compute_element_stiffness_matrix(i, KL)
            ! do j = 1,6
            !     do k = 1,6
            !         u = 
            !         v = 
            !     end do
            ! end do
        end do
        
    end subroutine s_compute_global_stiffness_matrix

    subroutine s_compute_element_stiffness_matrix(elm, Ke)

        integer :: elm, i, j
        real(kind(0d0)) :: jacD !< Jacobian determinant
        real(kind(0d0)), dimension(4,6) :: B !< strain matrix
        real(kind(0d0)), dimension(6,6) :: Ke !< element stiffness matrix

        !< intermitant calculation matrices
        real(kind(0d0)), dimension(4,6) :: I1
        real(kind(0d0)), dimension(6,6) :: I2

        real(kind(0d0)) :: rc

        rc = EC(elm,1)

        ! Initialize Ke
        do i = 1,6
            do j = 1,6
                Ke(i,j) = 0d0
            end do
        end do

        ! Perform Guassian Quadrature
        do i = 1,4
            call s_compute_b_matrix(B, elm, gx(i), gy(i))
            call s_compute_jacobian_determinant(jacD, elm, gx(i), gy(i))
            
            I1 = matmul(D,B*jacD)
            I2 = matmul(TRANSPOSE(B),I1)

            Ke = Ke + weights(i)*I2
        end do

        ! Convert to volume
        Ke = Ke*(2*pi*rc)

        if (debug == 1) then
            print*, "Local Stiffness Matrix of element: ", elm
            call s_print_array(Ke, 6, 6)
        end if

    end subroutine s_compute_element_stiffness_matrix

end module m_finite_elements