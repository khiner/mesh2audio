module m_math

    implicit none

    private; public :: matVecMul

    contains

    subroutine matVecMul(A, b, c, m, n)

        integer :: m, n !< matrix/vector dimensions
        real(kind(0d0)), dimension(m, n) :: A !< matrix
        real(kind(0d0)), dimension(n) :: b    !< vector
        real(kind(0d0)), dimension(n), intent(IN):: c    !< output

        integer :: i, j, k !< iterators

        do i = 1,m
            c(i) = 0d0
            do j = 1, n
                c(i) = c(i) + A(i, j)*b(j)
            end do
        end do

    end subroutine matVecMul

end module m_math