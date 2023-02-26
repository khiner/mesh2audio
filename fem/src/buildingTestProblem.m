clc;clear;close all

x = zeros(16,1);
y = zeros(16,1);
z = zeros(16,1);

figure("Position", [50 50 1500 500]); hold on
xlabel("x")
ylabel("y")
zlabel("z")

m = 1; %vertex number
fprintf("Vertex Locations \n")
subplot(1,3,1); hold on;
view(30, 7)
axis([0 5 0 5 0 5])
title("Node Locaitons")
for i = 1:4
    for j = 1:4
        for k = 1:4
            fprintf("%i %1.1f %1.1f %1.1f \n", m, i, j, k)
            scatter3(i, j, k, "b")
            m = m + 1;
        end
    end
end

m = 1; %edge number
fprintf("\n Edge Connectivity\n")


m = 1; %element number
fprintf("\n Element Connectivity\n")
for i = 1:2
    for j = 1:2
        for k = 1:2

        end
    end
end
