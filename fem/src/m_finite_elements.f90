module m_finite_elements

    !< dependencies
    use m_global_parameters
    use m_math

    implicit none

    private; public :: s_initialize_guass_quadrature, &
        s_compute_element_stiffness_matrix, &
        s_compute_global_stiffness_matrix, &
        s_compute_global_mass_matrix

    real(kind(0d0)), dimension(9) :: gx !< x guass-quadrature points
    real(kind(0d0)), dimension(9) :: gy !< y guass-quadrature points
    real(kind(0d0)), dimension(9) :: weights !< guass-quadrature weights

    real(kind(0d0)) :: pi = 3.141592653589793d0

contains

    subroutine s_initialize_guass_quadrature()

        real(kind(0d0)) :: c = 0.6
        gx(1) = -sqrt(c); gx(2) = -sqrt(c); gx(3) = -sqrt(c); gx(4) = 0d0
        gx(5) = 0d0     ; gx(6) = 0d0     ; gx(7) = sqrt(c) ; gx(8) = sqrt(c)
        gx(9) = sqrt(c)
        gy(1) = -sqrt(c); gy(2) = 0d0     ; gy(3) = sqrt(c) ; gy(4) = -sqrt(c)
        gy(5) = 0d0     ; gy(6) = sqrt(c) ; gy(7) = -sqrt(c); gy(8) = 0d0
        gy(9) = sqrt(c)
        
        c = 25d0/81d0
        weights(1) = c; weights(3) = c; weights(7) = c; weights(9) = c

        c = 40d0/81d0
        weights(2) = c; weights(4) = c; weights(6) = c; weights(8) = c

        c = 64d0/81d0
        weights(5) = c


    end subroutine s_initialize_guass_quadrature

    subroutine S_compute_global_stiffness_matrix()

        integer :: i, j, k
        real(kind(0d0)), dimension(6,6) :: KL !< element stiffness matrix
        integer, dimension(3) :: GN !< global node numbers

        do i = 1,ndofs
            do j = 1,ndofs
                S(i,j) = 0d0
            end do
        end do

        do i = 1, elements
            call s_compute_element_stiffness_matrix(i, KL)

            GN(1) = E(i, 1)
            GN(2) = E(i, 2)
            GN(3) = E(i, 3)

            do j = 1,3
                do k = 1,3
                    S(2*(GN(k)-1)+1,2*(GN(j)-1)+1)  = S(2*(GN(k)-1)+1,2*(GN(j)-1)+1) + KL(2*(k-1)+1, 2*(j-1)+1)
                    S(2*(GN(k)-1)+1,2*(GN(j)-1)+2)  = S(2*(GN(k)-1)+1,2*(GN(j)-1)+2) + KL(2*(k-1)+1, 2*(j-1)+2)
                    S(2*(GN(k)-1)+2,2*(GN(j)-1)+1)  = S(2*(GN(k)-1)+2,2*(GN(j)-1)+1) + KL(2*(k-1)+2, 2*(j-1)+1)
                    S(2*(GN(k)-1)+2,2*(GN(j)-1)+2)  = S(2*(GN(k)-1)+2,2*(GN(j)-1)+2) + KL(2*(k-1)+2, 2*(j-1)+2)
                end do
            end do
        end do

        if (debug == 1) then
            print*, "Gobal Stiffness Matrix"
            call s_print_array(S,ndofs,ndofs, 1e6)
        end if
        
    end subroutine s_compute_global_stiffness_matrix

    subroutine s_compute_global_mass_matrix()

        integer :: i, j, k
        real(kind(0d0)), dimension(6,6) :: ML
        integer, dimension(3) :: GN !< global node numbers

        do i = 1,ndofs
            do j = 1,ndofs
                M(i,j) = 0d0
            end do
        end do

        do i = 1,elements
            call s_compute_element_mass_matrix(i, ML)
            
            GN(1) = E(i, 1)
            GN(2) = E(i, 2)
            GN(3) = E(i, 3)

            do j = 1,3
                do k = 1,3
                    M(2*(GN(k)-1)+1,2*(GN(j)-1)+1)  = M(2*(GN(k)-1)+1,2*(GN(j)-1)+1) + ML(2*(k-1)+1, 2*(j-1)+1)
                    M(2*(GN(k)-1)+1,2*(GN(j)-1)+2)  = M(2*(GN(k)-1)+1,2*(GN(j)-1)+2) + ML(2*(k-1)+1, 2*(j-1)+2)
                    M(2*(GN(k)-1)+2,2*(GN(j)-1)+1)  = M(2*(GN(k)-1)+2,2*(GN(j)-1)+1) + ML(2*(k-1)+2, 2*(j-1)+1)
                    M(2*(GN(k)-1)+2,2*(GN(j)-1)+2)  = M(2*(GN(k)-1)+2,2*(GN(j)-1)+2) + ML(2*(k-1)+2, 2*(j-1)+2)
                end do
            end do
        end do

        if (debug == 1) then
            print*, "Gobal Mass Matrix"
            call s_print_array(M,ndofs,ndofs)
        end if

    end subroutine s_compute_global_mass_matrix

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
        do i = 1,9
            call s_compute_b_matrix(B, elm, gx(i), gy(i))
            call s_compute_jacobian_determinant(jacD, elm, gx(i), gy(i))
            !call s_print_array(B, 4, 6)
            I1 = matmul(D,B*jacD)
            I2 = matmul(TRANSPOSE(B),I1)

            Ke = Ke + weights(i)*I2
        end do

        ! Convert to volume
        Ke = Ke*(2*pi*rc)

        if (debug == 1) then
            print*, "Local Stiffness Matrix of element: ", elm
            call s_print_array(Ke, 6, 6, 1e6)
        end if

    end subroutine s_compute_element_stiffness_matrix

    subroutine s_compute_element_mass_matrix(elm, Me)

        integer :: elm, i, j

        real(kind(0d0)), dimension(2,6) :: N  !< shape function matrix matrix
        real(kind(0d0)), dimension(6,6) :: Me !< element mass matrix

        !< intermitant calculation matrices
        real(kind(0d0)), dimension(6,6) :: I1

        ! Initialize Me
        do i = 1,6
            do j = 1,6
                Me(i,j) = 0d0
            end do
        end do

        ! Perform Guassian Quadrature
        do i = 1,9
            call s_compute_N_matrix(N, gx(i), gy(i))

            I1 = matmul(transpose(N),N)

            Me = Me + weights(i)*I1

        end do

        ! Convert to volume
        Me = Me*(rho*2*pi*EC(elm,3)*EC(elm,1))

        if (debug == 1) then
            print*, "Local Mass Matrix of element: ", elm
            call s_print_array(Me, 6, 6)
        end if


    end subroutine s_compute_element_mass_matrix

end module m_finite_elements