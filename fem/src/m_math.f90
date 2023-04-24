module m_math

    use m_global_parameters

    implicit none

    private; public :: s_initialize_d, &
        s_compute_b_matrix, &
        s_compute_jacobian_determinant, &
        s_compute_N_matrix

contains

    subroutine s_initialize_d()

        real(kind(0d0)) :: c

        c = ym/((1+nu)*(1-2*nu)) !< constant for making things easier

        ! Row 1 of D
        D(1,1) = c*(1-nu)
        D(1,2) = c*nu
        D(1,3) = c*nu
        D(1,4) = 0d0
        ! Row 2 of D
        D(2,1) = c*nu
        D(2,2) = c*(1-nu)
        D(2,3) = c*nu
        D(2,4) = 0d0
        ! Row 3 of D
        D(3,1) = c*nu
        D(3,2) = c*nu
        D(3,3) = c*(1-nu)
        D(3,4) = 0d0
        ! Row 4 of D
        D(4,1) = 0d0
        D(4,2) = 0d0
        D(4,3) = 0d0
        D(4,4) = c*(1-2*nu)/2

    end subroutine s_initialize_d

    subroutine s_compute_b_matrix(B, elm, s, t)

        integer, intent(in) :: elm !< element number
        real(kind(0d0)), intent(in) :: s, t
        real(kind(0d0)), intent(out), dimension(4,6) :: B

        real(kind(0d0)) :: r1, z1, r2, z2, r3, z3 !< vertex coordinates
        real(kind(0d0)) :: rc !< r-centroid

        r1 = dofs(E(elm,1),1)
        r2 = dofs(E(elm,2),1)
        r3 = dofs(E(elm,3),1)
        
        z1 = dofs(E(elm,1),2)
        z2 = dofs(E(elm,2),2)
        z3 = dofs(E(elm,3),2)

        rc = EC(elm,1) 

        B(1,1) = (z3-z2)/(r3*(z2-z1) + r2*(z1-z3) + r1*(z3-z2))
        B(1,2) = 0d0
        B(1,3) = (z1-z3)/(r3*(z2-z1) + r2*(z1-z3) + r1*(z3-z2))
        B(1,4) = 0d0
        B(1,5) = (z1-z2)/(r3*(z1-z2) + r2*(z3-z1) + r1*(z2-z3))
        B(1,6) = 0d0

        B(2,1) = 0d0
        B(2,2) = (r2-r3)/(r3*(z2-z1) + r2*(z1-z3) + r1*(z3-z2))
        B(2,3) = 0d0
        B(2,4) = (r1-r3)/(r3*(z1-z2) + r2*(z3-z1) + r1*(z2-z3))
        B(2,5) = 0d0
        B(2,6) = (r1-r2)/(r3*(z2-z1) + r2*(z1-z3) + r1*(z3-z2))

        B(3,1) = (1+s)*(1+t)/(4*rc)
        B(3,2) = 0d0
        B(3,3) = (1-s)*(1+t)/(4*rc)
        B(3,4) = 0d0
        B(3,5) = (1-t)/(2*rc)
        B(3,6) = 0d0

        B(4,1) = B(2,2)
        B(4,2) = B(1,1)
        B(4,3) = B(2,4)
        B(4,4) = B(1,3)
        B(4,5) = B(2,6)
        B(4,6) = B(1,5)

    end subroutine s_compute_b_matrix

    subroutine s_compute_jacobian_determinant(jacD, elm, s, t)

        integer, intent(in) :: elm !< element number
        real(kind(0d0)), intent(in) :: s, t
        real(kind(0d0)), intent(out) :: jacD !< computed determinant

        real(kind(0d0)) :: r1, z1, r2, z2, r3, z3 !< vertex coordinates

        r1 = dofs(E(elm,1),1)
        r2 = dofs(E(elm,2),1)
        r3 = dofs(E(elm,3),1)
        
        z1 = dofs(E(elm,1),2)
        z2 = dofs(E(elm,2),2)
        z3 = dofs(E(elm,3),2)     

        jacD = (1d0/8d0)*(1 + t)*(r3*(z1-z2) + r2*(z3-z1) + r1*(z2-z3))

    end subroutine

    subroutine s_compute_N_matrix(N, s, t)

        real(kind(0d0)), dimension(2,6) :: N
        real(kind(0d0)) :: s, t

        N(1,1) = (1d0/4d0)*(1+s)*(1+t)
        N(1,2) = 0d0
        N(1,3) = (1d0/4d0)*(1-s)*(1+t)
        N(1,4) = 0d0
        N(1,5) = (1d0/2d0)*(1-t)
        N(1,6) = 0d0

        N(2,1) = 0d0
        N(2,2) = (1d0/4d0)*(1+s)*(1+t)
        N(2,3) = 0d0
        N(2,4) = (1d0/4d0)*(1-s)*(1+t)
        N(2,5) = 0d0
        N(2,6) = (1d0/2d0)*(1-t)

    end subroutine


end module m_math