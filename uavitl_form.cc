#include <iostream>
#include <vector>
#include <time.h>

#include "sim.hh"
#include "flyingmachine.hh"
#include "./quadrotor/quad_gnc.hh"
#include "./quadrotor/quad_sensors.hh"
#include "./formation/distance_formation.hh"
#include "./formation/position_formation.hh"
#include "./formation/bearing_formation.hh"

Eigen::VectorXf create_X_from_quads(std::vector<Quad_GNC*> *q, int m)
{
    Eigen::VectorXf X(m*q->size());

    int i = 0;
    for (std::vector<Quad_GNC*>::iterator it = q->begin();
            it != q->end(); ++it){
        Eigen::Vector3f X_i((*it)->get_X());
        X.segment(m*i, m) = X_i.segment(0, m);
        i++;
    }

    return X;
}

Eigen::VectorXf create_V_from_quads(std::vector<Quad_GNC*> *q, int m)
{
    Eigen::VectorXf V(m*q->size());

    int i = 0;
    for (std::vector<Quad_GNC*>::iterator it = q->begin();
            it != q->end(); ++it){
        Eigen::Vector3f V_i((*it)->get_V());
        V.segment(m*i, m) = V_i.segment(0, m);
        i++;
    }

    return V;
}

int main(int argc, char* argv[])
{
    std::string cfg;

    if(argc == 2){
        cfg = argv[1];
    }

    timespec ts;
    timespec tsleep;
    tsleep.tv_sec = 0;
    int last_step_time = -1;
    long dt = 166e5; // nanoseconds
    long time = 0;

    Eigen::Matrix3f J;
    J << 7.5e-3,      0,      0,
      0, 7.5e-3,      0,
      0,      0, 1.3e-2;
    float m = 1;
    float l = 0.23;
    //float kt = 3.13e-5;
    float kt = 2.99e-5;
    float km = 7.5e-7;
    float kp = 4;
    float kq = 4;
    float kr = 4;
    float k_xy = 1e-1;
    float k_vxy = 1;
    float k_vz = 1;
    float k_alt = 1e-1;
    float k_xi_g_v = 5e-1;
    float k_xi_g_e_alt = 1e-1;
    float k_xi_CD_e_v = 1e-3;

    int num_quads = 3;

    std::string q_ip("127.0.0.1");
    int q_udp_xplane_in[3] = {50000, 51000, 52000};
    int q_udp_xplane_out[3] = {60001, 61001, 62001};

    std::vector<Flyingmachine> quads;
    std::vector<Quad_GNC*> quads_gnc;

    for(int i = 0; i < num_quads; i++){
        Sim *q_xp = 
            new Sim(q_ip, q_udp_xplane_in[i], q_udp_xplane_out[i], XPLANE);
        Quad_Sensors *q_sen = new Quad_Sensors(i+1, q_xp);
        Quad_GNC *q_gnc = new Quad_GNC(i+1, q_xp, q_sen);

        q_gnc->set_physical_variables(J, m, l);
        q_gnc->set_motor_model(kt, km);
        q_gnc->set_attitude_gains(kp, kq, kr);
        q_gnc->set_control_gains(k_xy, k_vxy, k_vz, k_alt);
        q_gnc->set_xi_g_gains(k_xi_g_v, k_xi_g_e_alt);
        q_gnc->set_xi_CD_gains(k_xi_CD_e_v);

        Flyingmachine q(q_xp, dynamic_cast<Sensors*>(q_sen),
                dynamic_cast<GNC*>(q_gnc));

        quads.push_back(q);
        quads_gnc.push_back(q_gnc);
    }

    // Formation Control
    // Distance-based
    int fcm = 2;
    int fcl = 1;
    float c_shape = 2e-1;
    float c_vel = 2e-2;
    float k_v_hat = 0;
    float k_mu_hat = 0;
    Eigen::VectorXf fcd(3);
    Eigen::VectorXf mu(3);
    Eigen::VectorXf tilde_mu(3);
    Eigen::MatrixXf B(3, 3);
    B << 1,  0,  1,
        -1,  1,  0,
         0, -1, -1;

    float a = 150;
    mu << 0, 0, 0;
    tilde_mu << 0, 0, 0;
    fcd << a, a, a;
    DistanceFormation df(fcm, fcl, fcd, mu, tilde_mu, B, c_shape, c_vel, 
            k_v_hat, k_mu_hat);

    // Position-based only for 1-2
    Eigen::VectorXf zd(6);
    Eigen::MatrixXf Bpr(3, 3);

    Bpr << 1,  0,  0,
          -1,  0,  0,
           0,  0,  0;
    zd << 0, a, 0, 0, 0, 0;
    PositionFormation pfr(fcm, zd, Bpr, 5*c_shape, c_vel);

    // Setting control
    for (std::vector<Quad_GNC*>::iterator it = quads_gnc.begin();
            it != quads_gnc.end(); ++it){
        (*it)->set_xyz_zero(0.824756, 0.198016, 576.5);
        (*it)->set_yaw_d(M_PI);
        (*it)->set_active_controller(V_2D_ALT);
        (*it)->set_v_2D_alt(0, 0, -600);
    }

    for(;;){
        clock_gettime(CLOCK_REALTIME, &ts);
        last_step_time = ts.tv_nsec;

        for (std::vector<Flyingmachine>::iterator it = quads.begin();
                it != quads.end(); ++it)
            it->update(time);

        time += dt;

        if(time >= 15e9)
        {
            Eigen::VectorXf X = create_X_from_quads(&quads_gnc, fcm);
         //   Eigen::VectorXf V = create_V_from_quads(&quads_gnc, fcm);

            Eigen::VectorXf Us = df.get_u_vel(X);
            Eigen::VectorXf Uo = pfr.get_u_vel(X);

            Eigen::VectorXf U = Us;


            int i = 0;
            for (std::vector<Quad_GNC*>::iterator it = quads_gnc.begin();
                    it != quads_gnc.end(); ++it){
                (*it)->set_v_2D_alt(U(i*2), U(i*2+1), -600);
                (*it)->log(time*1e-9);
                i++;
            }

            df.log(time*1e-9);
        }

        clock_gettime(CLOCK_REALTIME, &ts);
        tsleep.tv_nsec = dt - (ts.tv_nsec - last_step_time);
        nanosleep(&tsleep, NULL);
    }

    return 0;
}
