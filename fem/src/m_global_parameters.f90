module m_global_parameters

    implicit none

    integer :: vertices
    integer :: elements
    integer :: edges

    integer, parameter :: debug = 0

    real(kind(0d0)), allocatable, dimension(:,:) :: dofs  !< degrees of freedom
    real(kind(0d0)), allocatable, dimension(:, :) :: M  !< Mass matrix
    real(kind(0d0)), allocatable, dimension(:, :) :: S  !< Stiffness matrix
    real(kind(0d0)), allocatable, dimension(:, :) :: EC !< double element centroids
    integer, allocatable, dimension(:, :) :: E !< Element connectivity mnatrix
    real(kind(0d0)), dimension(4,4) :: D !< stress strain matrix
    real(kind(0d0)), dimension(2,6) :: N !< (x,y) to x(s,t) y(s,t) matrix

    real(kind(0d0)) :: ym  !< youngs modulus
    real(kind(0d0)) :: nu  !< poissons ratio
    real(kind(0d0)) :: rho !< density 
    integer :: ndofs


    public

contains

    subroutine s_initialize_global_parameters()

    end subroutine

    subroutine s_print_array(A, m, n, div)

        real(kind(0d0)), dimension(m,n) :: A
        integer :: i, j
        integer :: m, n
        real :: c
        real, optional :: div

        if (present(div)) then 
            c = div
        else
            c = 1
        endif
        
        do i = 1,m
            do j = 1,n
                write(*,fmt="(F8.2)",advance="no") A(i,j)/c
            end do
            write(*, fmt="(A1)") " "
        end do
        write(*, fmt="(A1)") " "

    end subroutine

    subroutine s_print_int_array(A, m, n)

        integer, dimension(m,n) :: A
        integer :: i, j
        integer :: m, n
        
        do i = 1,m
            do j = 1,n
                write(*,fmt="(I5)",advance="no") A(i,j)
            end do
            write(*, fmt="(A1)") " "
        end do
        write(*, fmt="(A1)") " "

    end subroutine

end module