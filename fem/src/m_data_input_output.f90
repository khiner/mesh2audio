module m_data_input_output

    !< Dependencies 
    use m_global_parameters
    use stringmod
    use precmod

    implicit none
    
    integer :: Nv = 1  !< dof iterator
    integer :: Nel = 1 !< element iterator
    integer :: Ned     !< edge iterator
    integer:: io       !< iostat variable
    character (len=100) :: filecontents, msg, lineType !< strings for stuff

    character filename*50
    character arg*50

    private;  public :: s_read_obj_file, &
        s_write_output

contains

    !< This subroutine reads the mesh file and populates lots of matrices
    subroutine s_read_obj_file()

        integer :: i, j, k !< standard loop iterators
        real(kind(0d0)) :: r1, z1, r2, z2, r3, z3 !< vertex coordinates

        call getarg(1, filename) !< gets filename from CLA
        call getarg(2, arg)       !< gets youngs modules
        read(arg, *) ym
        call getarg(3, arg)       !< gets poissons ratio
        read(arg, *) nu

        if (debug == 1) then
            print*, ""
            print*, "Material Properties"
            print*, "     Youngs Modulus: ", ym
            print*, "     Poisson's Ratio: ", nu
            print*, ""
        end if

        open (unit = 1, file = trim(filename)//".obj", iostat=io)

        !< read raw data
        do
            read(1, "(a)", iostat=io, iomsg=msg) filecontents
            if (io/=0) exit

            call split(filecontents, " ", lineType)

            if (lineType == "#") then
                call split(filecontents, " ", lineType)
                !< get vertex count for allocating matrices             
                if (lineType == "Vertices:") then
                    call split(filecontents, " ", lineType)
                    read(lineType, *) vertices
                    Ndofs = vertices*2
                    allocate (dofs(1:vertices,2))
                    allocate (M(1:vertices*2,1:vertices*2))
                    allocate (S(1:vertices*2,1:vertices*2))
                end if


                !< get element count for allocating matrices
                if (linetype == "Faces:") then
                    call split(filecontents, " ", lineType)
                    read(lineType, *) elements
                    allocate (E(1:elements, 3))
                    allocate (EC(1:elements, 2))
                end if
            end if

            !< if line corresponds to a vertex location
            if (lineType == "v") then
                do j = 1, 2
                    call split(filecontents, " ", lineType)
                    read(lineType, *) dofs(Nv,j)
                end do
                Nv = Nv + 1
            end if

            !< if lline corresponds to a face element specification
            if (lineType == "f") then
                do j = 1, 3
                    call split(filecontents, " ", lineType)
                    read(lineType, *) E(Nel, j)
                end do
                Nel = Nel + 1
            end if

        end do

        close(1)

        !< populate element centroids
        do i = 1, elements

            r1 = dofs(E(i,1),1)
            r2 = dofs(E(i,2),1)
            r3 = dofs(E(i,3),1)
            
            z1 = dofs(E(i,1),2)
            z2 = dofs(E(i,2),2)
            z3 = dofs(E(i,3),2)
            EC(i,1) = (r1 + r2 + r3)/3
            EC(i,2) = (z1 + z2 + z3)/3

        end do

        if (debug == 1) then
            print*, "Element Connectivity array"
            call s_print_int_array(E, elements, 3)
            print*, "Element Centers array"
            call s_print_array(EC, elements, 2)
            print*, "DOFs array"
            call s_print_array(dofs, vertices, 2)
        end if

    end subroutine s_read_obj_file

    subroutine s_write_output()

        integer :: i, j
        character f*50

        !< Write Stiffness Matrix
        f = trim(filename)//"_K.out"
        open(1, file = trim(f), iostat = io)

        do i = 1,Ndofs
            do j = 1,Ndofs
                if (abs(S(i,j)) > 1e-10) then
                    write(1,"(I6,A,I6,A,F32.8)") i, ",", j, ",", S(i,j)
                end if
            end do
        end do

        close(1)

        !< Write Mass Matrix
        f = trim(filename)//"_M.out"
        open(1, file = trim(f), iostat = io)

        do i = 1,Ndofs
            do j = 1,Ndofs
                if (abs(M(i,j)) > 1e-10) then
                    write(1,"(I6,A,I6,A,F32.8)") i, ",", j, ",", M(i,j)
                end if
            end do
        end do

        close(1)

    end subroutine s_write_output

end module m_data_input_output