function [R, U] = response_time(T,E)
R = zeros(size(T));
for i = 1:length(R)
    R(i) = sum(E(1:i));
    R_old = 0;
    j = 1:i-1;
    while abs(R(i)-R_old) > 10*eps(R(i))
        R_old = R(i);
        R(i) = E(i) + sum(ceil(R(i)./T(j)) .* E(j));
    end
end
U = sum(E./T); %CPU utilization